/*
 * Msh - Prompt rendering.
 *
 * Supports two modes:
 *   - Default: user@host:path$ with ANSI colors, ~ for $HOME.
 *   - Custom:   if $MSH_PROMPT or st->prompt is set, expand escape sequences.
 *
 * Escape sequences (custom mode):
 *   \u  user       \h  host (short)
 *   \H  host (fqdn) \w  cwd (with ~)
 *   \W  basename    \$
 *   \d  date        \t  time
 *   \n  newline     \\
 *   \e  ESC
 */
#include "msh/prompt.h"
#include "msh/util.h"
#include "msh/var.h"
#include "msh/job.h"
#include "msh/line_edit.h"
#include <sys/utsname.h>
#include <unistd.h>
#include <time.h>

static const char *short_hostname(char *buf, size_t n)
{
    static char cache[256];
    static int init = 0;
    if (!init) {
        if (gethostname(cache, sizeof(cache) - 1) != 0) snprintf(cache, sizeof(cache), "host");
        char *dot = strchr(cache, '.');
        if (dot) *dot = '\0';
        init = 1;
    }
    (void)buf; (void)n;
    return cache;
}

static const char *pretty_cwd(void)
{
    static char buf[MSH_MAX_LINE];
    if (!getcwd(buf, sizeof(buf) - 1)) snprintf(buf, sizeof(buf), "?");
    const char *home = getenv("HOME");
    if (home && msh_strstarts(buf, home)) {
        char tmp[MSH_MAX_LINE];
        snprintf(tmp, sizeof(tmp), "~%s", buf + strlen(home));
        snprintf(buf, sizeof(buf), "%s", tmp);
    }
    return buf;
}

/* Read first line of `.git/HEAD` if present. Returns "" otherwise. */
static const char *git_branch(void)
{
    static char branch[128];
    static int init = -1;
    if (init == 0) return branch;
    init = 0;
    branch[0] = '\0';
    char path[MSH_MAX_LINE];
    snprintf(path, sizeof(path), "%s/.git/HEAD", pretty_cwd());
    FILE *f = fopen(path, "r");
    if (!f) { init = 0; return branch; }
    char line[256];
    if (fgets(line, sizeof(line), f)) {
        char *p = strstr(line, "refs/heads/");
        if (p) {
            p += 11;
            size_t l = strlen(p);
            while (l && (p[l-1] == '\n' || p[l-1] == '\r')) p[--l] = '\0';
            snprintf(branch, sizeof(branch), "%s", p);
        }
    }
    fclose(f);
    return branch;
}

static void append(char **buf, size_t *cap, size_t *n, const char *s)
{
    size_t sl = strlen(s);
    if (*n + sl + 1 >= *cap) { *cap = (*n + sl + 1) * 2; *buf = msh_xrealloc(*buf, *cap); }
    memcpy(*buf + *n, s, sl); *n += sl;
    (*buf)[*n] = '\0';
}

static char *expand_prompt(const char *fmt)
{
    char *buf = msh_xmalloc(256);
    size_t cap = 256, n = 0;
    for (const char *p = fmt; *p; p++) {
        if (*p != '\\') { char tmp[2] = {*p, 0}; append(&buf, &cap, &n, tmp); continue; }
        p++;
        switch (*p) {
            case 'u': append(&buf, &cap, &n, getenv("USER") ? getenv("USER") : "user"); break;
            case 'h': append(&buf, &cap, &n, short_hostname(NULL, 0)); break;
            case 'H': {
                struct utsname u;
                if (uname(&u) == 0) append(&buf, &cap, &n, u.nodename);
                else append(&buf, &cap, &n, "host");
                break;
            }
            case 'w': append(&buf, &cap, &n, pretty_cwd()); break;
            case 'W': {
                const char *cwd = pretty_cwd();
                const char *slash = strrchr(cwd, '/');
                append(&buf, &cap, &n, slash ? slash + 1 : cwd);
                break;
            }
            case '$': append(&buf, &cap, &n, geteuid() == 0 ? "#" : "$"); break;
            case 'd': {
                time_t t = time(NULL);
                struct tm *tm = localtime(&t);
                char tbuf[64];
                strftime(tbuf, sizeof(tbuf), "%a %b %d", tm);
                append(&buf, &cap, &n, tbuf);
                break;
            }
            case 't': {
                time_t t = time(NULL);
                struct tm *tm = localtime(&t);
                char tbuf[64];
                strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm);
                append(&buf, &cap, &n, tbuf);
                break;
            }
            case 'n': append(&buf, &cap, &n, "\n"); break;
            case 'e': append(&buf, &cap, &n, "\033"); break;
            case '\\': append(&buf, &cap, &n, "\\"); break;
            case 'g': {
                const char *b = git_branch();
                if (*b) {
                    char tmp[160];
                    snprintf(tmp, sizeof(tmp), "\033[1;36m(%s)\033[0m", b);
                    append(&buf, &cap, &n, tmp);
                }
                break;
            }
            case '[':
            case ']': {
                /* skip non-printing region markers */
                while (*p && *p != *p) p++;
                break;
            }
            default:
                if (*p) { char tmp[2] = {*p, 0}; append(&buf, &cap, &n, tmp); }
                break;
        }
    }
    return buf;
}

static void build_default_prompt(char *out, size_t max) __attribute__((unused));
static void build_default_prompt(char *out, size_t max)
{
    const char *user = getenv("USER");
    if (!user) user = "user";
    const char *host = short_hostname(NULL, 0);
    const char *cwd  = pretty_cwd();
    int status = msh_get_state()->last_status;

    char shortcwd[MSH_MAX_LINE];
    snprintf(shortcwd, sizeof(shortcwd), "%.*s", 200, cwd);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    if (status == 0) {
        snprintf(out, max,
                 "\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m\033[1;36m%s\033[0m$ ",
                 user, host, shortcwd, "");
    } else {
        snprintf(out, max,
                 "\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m \033[31m[%d]\033[0m$ ",
                 user, host, shortcwd, status);
    }
#pragma GCC diagnostic pop
}

void msh_prompt_init(void) {}

void msh_prompt_set(const char *p)
{
    msh_state_t *st = msh_get_state();
    if (p) snprintf(st->prompt, sizeof(st->prompt), "%s", p);
}

static const char *default_template(void)
{
    /* This template is expanded on every prompt invocation so cwd, git,
     * and exit-status are always fresh. */
    static const char tmpl[] =
        "\\033[1;32m\\u@\\h\\033[0m:"
        "\\033[1;34m\\w\\033[0m"
        "\\033[1;36m\\g\\033[0m"
        "\\033[31m\\$\\033[0m "
        "\0"; /* placeholder; rebuilt below */
    return tmpl;
}

char *msh_prompt_get(void)
{
    msh_state_t *st = msh_get_state();
    /* Check $MSH_PROMPT first */
    const char *env = getenv("MSH_PROMPT");
    if (env && *env) return expand_prompt(env);

    /* Custom template set via msh_prompt_set: contains '\' sequences */
    if (st->prompt[0] && strchr(st->prompt, '\\')) return expand_prompt(st->prompt);

    /* Default template — expand every time so cwd/status are fresh */
    const char *tpl =
        "\\033[1;32m\\u@\\h\\033[0m:\\033[1;34m\\w\\033[0m\\033[1;36m\\g\\033[0m$ ";
    /* If last status != 0, show it */
    if (st->last_status != 0) {
        tpl = "\\033[1;32m\\u@\\h\\033[0m:\\033[1;34m\\w\\033[0m \\033[31m["
              "\\033[0m\\033[31m]\\033[0m$ ";
        /* we'll inject status separately */
    }
    char *expanded = expand_prompt(tpl);
    /* Inject status if needed */
    if (st->last_status != 0) {
        char with_status[MSH_MAX_PROMPT];
        /* Replace the first \033[31m]\033[0m with the status number */
        char *p = strstr(expanded, "\033[31m]\033[0m");
        if (p) {
            size_t before = p - expanded;
            snprintf(with_status, sizeof(with_status), "%.*s%d%s",
                     (int)before, expanded, st->last_status, p + strlen("\033[31m]\033[0m"));
        } else {
            snprintf(with_status, sizeof(with_status), "%s", expanded);
        }
        snprintf(st->prompt, sizeof(st->prompt), "%s", with_status);
    } else {
        snprintf(st->prompt, sizeof(st->prompt), "%s", expanded);
    }
    free(expanded);
    (void)default_template;
    return st->prompt;
}

int msh_prompt_input(char *buf, size_t max)
{
    char *prompt = msh_prompt_get();
    return msh_line_edit_read(buf, max, prompt);
}