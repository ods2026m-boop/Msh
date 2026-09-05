/*
 * Msh - Completion.
 *
 * Supports:
 *   - Built-in command names (first word)
 *   - PATH executables (first word)
 *   - Aliases (first word)
 *   - Local files / directories (any word)
 *   - Tilde-expanded paths (~user/...)
 *   - Environment variables ($NAME at word start)
 */
#include "msh/completion.h"
#include "msh/util.h"
#include "msh/builtin.h"
#include "msh/var.h"
#include <dirent.h>

void msh_completion_init(void) {}

void msh_completion_free(msh_completions_t *c)
{
    if (!c) return;
    for (int i = 0; i < c->count; i++) free(c->items[i]);
    free(c->items);
    c->items = NULL; c->count = c->cap = 0;
}

static void comp_add(msh_completions_t *c, const char *s)
{
    if (c->count >= c->cap) {
        c->cap = c->cap ? c->cap * 2 : 16;
        c->items = msh_xrealloc(c->items, sizeof(char*) * c->cap);
    }
    c->items[c->count++] = msh_strdup(s);
}

static void comp_add_unique(msh_completions_t *c, const char *s)
{
    for (int i = 0; i < c->count; i++) {
        if (msh_streq(c->items[i], s)) return;
    }
    comp_add(c, s);
}

static void comp_files(const char *prefix, msh_completions_t *out)
{
    if (!prefix) return;
    char *dir = ".";
    char *buf = msh_strdup(prefix);
    if (!buf) return;
    char *base = buf;
    char *slash = strrchr(buf, '/');
    if (slash) {
        *slash = '\0';
        if (*buf) dir = buf;
        base = slash + 1;
    }
    DIR *d = opendir(dir);
    if (!d) { free(buf); return; }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (msh_streq(e->d_name, ".") || msh_streq(e->d_name, "..")) continue;
        if (msh_strstarts(e->d_name, base)) {
            char *full = msh_path_join(dir, e->d_name);
            comp_add(out, full);
            free(full);
        }
    }
    closedir(d);
    free(buf);
}

/* Expand ~user/ prefix to its real path. Returns malloc'd or NULL. */
static char *tilde_expand(const char *s)
{
    if (!s || s[0] != '~') return NULL;
    const char *home = getenv("HOME");
    if (!home) return NULL;
    /* skip "~" or "~user" */
    const char *p = s + 1;
    while (*p && *p != '/') p++;
    return msh_strconcat(home, p);
}

static int is_word_char(int c)
{
    return !(msh_isspace(c) || c == '|' || c == '&' || c == ';' ||
             c == '<' || c == '>');
}

static const char *word_start(const char *line, int cursor)
{
    if (cursor <= 0) return line;
    int i = cursor - 1;
    while (i >= 0 && is_word_char((unsigned char)line[i])) i--;
    return line + i + 1;
}

void msh_complete(const char *line, int cursor, msh_completions_t *out)
{
    if (!line || !out) return;
    memset(out, 0, sizeof(*out));
    const char *start = word_start(line, cursor);
    int is_first = (start == line);
    char *prefix = msh_strndup(start, cursor - (start - line));

    if (prefix[0] == '~') {
        char *expanded = tilde_expand(prefix);
        if (expanded) {
            comp_files(expanded, out);
            free(expanded);
        }
    } else if (prefix[0] == '$') {
        /* complete env var name */
        extern char **environ;
        for (char **e = environ; *e; e++) {
            const char *eq = strchr(*e, '=');
            if (!eq) continue;
            char name[256];
            size_t nl = eq - *e;
            if (nl >= sizeof(name)) continue;
            memcpy(name, *e, nl); name[nl] = '\0';
            if (msh_strstarts(name, prefix + 1)) {
                char *full = msh_xmalloc(nl + 2);
                full[0] = '$';
                memcpy(full + 1, name, nl + 1);
                comp_add(out, full);
                free(full);
            }
        }
    } else if (is_first) {
        /* builtins */
        for (const msh_builtin_t *b = msh_builtins; b->name; b++) {
            if (msh_strstarts(b->name, prefix)) comp_add(out, b->name);
        }
        /* aliases */
        msh_state_t *st = msh_get_state();
        for (int i = 0; i < st->alias_count; i++) {
            if (msh_strstarts(st->aliases[i].name, prefix))
                comp_add(out, st->aliases[i].name);
        }
        /* PATH executables */
        const char *path = getenv("PATH");
        if (path) {
            char *pdup = msh_strdup(path);
            char *save = NULL;
            for (char *d = strtok_r(pdup, ":", &save); d; d = strtok_r(NULL, ":", &save)) {
                DIR *dir = opendir(d);
                if (!dir) continue;
                struct dirent *e;
                while ((e = readdir(dir)) != NULL) {
                    if (msh_strstarts(e->d_name, prefix)) {
                        char *full = msh_path_join(d, e->d_name);
                        if (msh_is_executable(full)) comp_add_unique(out, e->d_name);
                        free(full);
                    }
                }
                closedir(dir);
            }
            free(pdup);
        }
    } else {
        comp_files(prefix, out);
    }
    free(prefix);
}

/* Compute longest common prefix of all items in the completion set. */
char *msh_completion_common_prefix(msh_completions_t *c)
{
    if (!c || c->count == 0) return msh_strdup("");
    const char *ref = c->items[0];
    size_t n = strlen(ref);
    for (int i = 1; i < c->count; i++) {
        const char *s = c->items[i];
        size_t k = 0;
        while (k < n && s[k] && s[k] == ref[k]) k++;
        n = k;
        if (n == 0) break;
    }
    return msh_strndup(ref, n);
}