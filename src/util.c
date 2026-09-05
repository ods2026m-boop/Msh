/*
 * Msh - Utility functions
 * Memory allocation helpers, string manipulation, path helpers.
 */
#include "msh/util.h"
#include <stdint.h>

static msh_state_t g_state;
static int g_state_inited = 0;

msh_state_t *msh_get_state(void) { return &g_state; }

void msh_state_init(void)
{
    if (g_state_inited) return;
    memset(&g_state, 0, sizeof(g_state));
    g_state.exit_code   = 0;
    g_state.last_status = 0;
    g_state.next_job_id = 1;
    g_state.history_capacity = MSH_MAX_HISTORY;
    g_state.history = msh_xcalloc(g_state.history_capacity, sizeof(msh_hist_entry_t));
    g_state.alias_capacity = MSH_MAX_ALIASES;
    g_state.aliases = msh_xcalloc(g_state.alias_capacity, sizeof(msh_alias_t));
    g_state_inited = 1;
}

void msh_state_cleanup(void)
{
    if (!g_state_inited) return;
    free(g_state.cwd);
    free(g_state.home);
    free(g_state.user);
    free(g_state.host);
    for (int i = 0; i < g_state.history_count; i++) free(g_state.history[i].line);
    free(g_state.history);
    for (int i = 0; i < g_state.alias_count; i++) {
        free(g_state.aliases[i].name);
        free(g_state.aliases[i].value);
    }
    free(g_state.aliases);
    msh_job_t *j = g_state.jobs_head;
    while (j) {
        msh_job_t *n = j->next;
        free(j->cmdline);
        free(j);
        j = n;
    }
    extern void msh_array_cleanup_all(void);
    msh_array_cleanup_all();
    extern void msh_function_clear_all(void);
    msh_function_clear_all();
    for (int i = 0; i < 9; i++) {
        free(g_state.pos_params[i]);
        g_state.pos_params[i] = NULL;
    }
    g_state_inited = 0;
}

void msh_error(const char *fmt, ...)
{
    fprintf(stderr, "msh: ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

void msh_perror(const char *prefix)
{
    fprintf(stderr, "msh: %s: %s\n", prefix ? prefix : "", strerror(errno));
}

/* ---- Memory allocation ---- */

void *msh_xmalloc(size_t size)
{
    if (size == 0) size = 1;
    void *p = malloc(size);
    if (!p) {
        void *fb = msh_sbrk_alloc(size);
        if (!fb) { fprintf(stderr, "msh: out of memory\n"); exit(1); }
        return fb;
    }
    return p;
}

void *msh_xcalloc(size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0) { nmemb = 1; size = 1; }
    void *p = calloc(nmemb, size);
    if (!p) {
        p = msh_xmalloc(nmemb * size);
        memset(p, 0, nmemb * size);
        return p;
    }
    return p;
}

void *msh_xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (!p && size != 0) {
        fprintf(stderr, "msh: out of memory\n");
        exit(1);
    }
    return p;
}

void *msh_sbrk_alloc(size_t size)
{
    /* Simple bump allocator fallback using sbrk when malloc fails.
     * Aligned to 16-byte boundary; never freed individually. */
    static char *brk_base = NULL;
    static size_t brk_off = 0;
    if (!brk_base) {
        brk_base = (char *)sbrk(0);
    }
    /* align */
    size_t align = 16;
    size_t cur = (size_t)brk_base + brk_off;
    size_t pad = (align - (cur % align)) % align;
    brk_off += pad;
    /* sbrk more memory if needed */
    char *cur_brk = (char *)sbrk(0);
    if (brk_off + size > (size_t)(cur_brk - brk_base)) {
        sbrk((long)(brk_off + size - (size_t)(cur_brk - brk_base) + 4096));
    }
    void *p = brk_base + brk_off;
    brk_off += size;
    return p;
}

/* ---- String utilities ---- */

char *msh_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = msh_xmalloc(n);
    memcpy(p, s, n);
    return p;
}

char *msh_strndup(const char *s, size_t n)
{
    if (!s) return NULL;
    size_t len = strnlen(s, n);
    char *p = msh_xmalloc(len + 1);
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

char *msh_strconcat(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char *p = msh_xmalloc(la + lb + 1);
    memcpy(p, a, la);
    memcpy(p + la, b, lb);
    p[la + lb] = '\0';
    return p;
}

int msh_strcmp(const char *a, const char *b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a, b);
}

int msh_streq(const char *a, const char *b) { return msh_strcmp(a, b) == 0; }

int msh_strstarts(const char *s, const char *prefix)
{
    if (!s || !prefix) return 0;
    size_t lp = strlen(prefix);
    return strncmp(s, prefix, lp) == 0;
}

int msh_strends(const char *s, const char *suffix)
{
    if (!s || !suffix) return 0;
    size_t ls = strlen(s), lf = strlen(suffix);
    return lf <= ls && memcmp(s + ls - lf, suffix, lf) == 0;
}

char *msh_strtrim(char *s)
{
    if (!s) return NULL;
    while (*s && msh_isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && msh_isspace((unsigned char)*end)) { *end = '\0'; end--; }
    return s;
}

char *msh_strip_quotes(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    if (n >= 2 && ((s[0] == '"' && s[n-1] == '"') ||
                   (s[0] == '\'' && s[n-1] == '\''))) {
        char *p = msh_xmalloc(n - 1);
        memcpy(p, s + 1, n - 2);
        p[n - 2] = '\0';
        return p;
    }
    return msh_strdup(s);
}

int msh_isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
int msh_isalnum(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                (c >= '0' && c <= '9') || c == '_'; }
int msh_isdigit(int c) { return c >= '0' && c <= '9'; }

/* ---- Splitting ---- */

char **msh_strsplit(const char *s, const char *delim, int *count)
{
    if (!s) { if (count) *count = 0; return NULL; }
    int cap = 8, n = 0;
    char **out = msh_xmalloc(sizeof(char*) * cap);
    const char *p = s;
    while (*p) {
        while (*p && strchr(delim, *p)) p++;
        if (!*p) break;
        const char *q = p;
        while (*q && !strchr(delim, *q)) q++;
        if (n >= cap) { cap *= 2; out = msh_xrealloc(out, sizeof(char*) * cap); }
        out[n++] = msh_strndup(p, q - p);
        p = q;
    }
    if (count) *count = n;
    if (n >= cap) { cap++; out = msh_xrealloc(out, sizeof(char*) * cap); }
    out[n] = NULL;
    return out;
}

char **msh_strsplit_keep(const char *s, const char *delim, int *count)
{
    return msh_strsplit(s, delim, count);
}

void msh_strfreev(char **argv)
{
    if (!argv) return;
    for (int i = 0; argv[i]; i++) free(argv[i]);
    free(argv);
}

/* ---- File/path utilities ---- */

int msh_file_exists(const char *path)
{
    if (!path) return 0;
    struct stat st;
    return stat(path, &st) == 0;
}

int msh_is_dir(const char *path)
{
    if (!path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

int msh_is_executable(const char *path)
{
    if (!path) return 0;
    if (access(path, X_OK) != 0) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

char *msh_path_join(const char *dir, const char *name)
{
    if (!dir) dir = "";
    if (!name) name = "";
    size_t ld = strlen(dir), ln = strlen(name);
    int need_sep = ld > 0 && dir[ld-1] != '/';
    char *p = msh_xmalloc(ld + need_sep + ln + 1);
    memcpy(p, dir, ld);
    if (need_sep) p[ld++] = '/';
    memcpy(p + ld, name, ln);
    p[ld + ln] = '\0';
    return p;
}

char *msh_basename(const char *path)
{
    if (!path) return NULL;
    const char *p = strrchr(path, '/');
    if (!p) return msh_strdup(path);
    if (p[1] == '\0' && p != path) {
        /* trailing slash */
        const char *q = p - 1;
        while (q > path && *q != '/') q--;
        if (*q == '/') return msh_strndup(q + 1, p - q - 1);
        return msh_strdup(path);
    }
    return msh_strdup(p + 1);
}

char *msh_dirname(const char *path)
{
    if (!path) return NULL;
    const char *p = strrchr(path, '/');
    if (!p) return msh_strdup(".");
    if (p == path) return msh_strdup("/");
    return msh_strndup(path, p - path);
}

char *msh_abspath(const char *path)
{
    if (!path) return NULL;
    if (path[0] == '/') return msh_strdup(path);
    char *cwd = msh_xmalloc(MSH_MAX_LINE);
    if (!getcwd(cwd, MSH_MAX_LINE)) { free(cwd); return msh_strdup(path); }
    char *full = msh_path_join(cwd, path);
    free(cwd);
    return full;
}

ssize_t msh_read_line(int fd, char *buf, size_t max)
{
    ssize_t total = 0;
    while (total < (ssize_t)max - 1) {
        ssize_t r = read(fd, buf + total, 1);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break;
        if (buf[total] == '\n') { total++; break; }
        total++;
    }
    buf[total] = '\0';
    return total;
}

ssize_t msh_write_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, buf + off, len - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += w;
    }
    return (ssize_t)off;
}