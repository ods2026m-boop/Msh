/*
 * Msh - Variable, tilde, arithmetic expansion.
 *
 * Special variables handled here:
 *   $?  last exit status
 *   $$  shell pid
 *   $!  last background job pid
 *   $-  current shell option flags
 *   $#  number of positional parameters (0 for non-script shell)
 *   $@, $*  positional parameters
 *   $0  shell name
 *   $1..$9  positional parameters
 *
 * Arithmetic $((expr)) is delegated to strtol on simple integer expressions.
 * Command substitution $(cmd) is executed via fork/pipe.
 */
#include "msh/expand.h"
#include "msh/util.h"
#include "msh/var.h"
#include "msh/job.h"
#include "msh/lexer.h"
#include "msh/parser.h"
#include "msh/exec.h"
#include "msh/compound.h"
#include <wordexp.h>
#include <stdlib.h>

extern int msh_opt_nullglob;
extern int msh_opt_extglob;
extern int msh_opt_dotglob;
extern int msh_opt_nocaseglob;
extern int msh_opt_errexit;
extern int msh_opt_xtrace;

static void append_chars(char **buf, size_t *cap, size_t *n, const char *s, size_t len)
{
    if (*n + len + 1 >= *cap) {
        size_t newcap = (*n + len + 1) * 2;
        char *nb = msh_xrealloc(*buf, newcap);
        *buf = nb; *cap = newcap;
    }
    memcpy(*buf + *n, s, len);
    *n += len;
    (*buf)[*n] = '\0';
}

static void append_str(char **buf, size_t *cap, size_t *n, const char *s)
{
    append_chars(buf, cap, n, s, strlen(s));
}

static const char *special_var(const char *name, char *out, size_t outsize)
{
    msh_state_t *st = msh_get_state();
    if (msh_streq(name, "?")) { snprintf(out, outsize, "%d", st->last_status); return out; }
    if (msh_streq(name, "$")) { snprintf(out, outsize, "%d", (int)getpid()); return out; }
    if (msh_streq(name, "!")) {
        /* last bg job's pgid */
        msh_job_t *j = msh_job_last_or_current();
        snprintf(out, outsize, "%d", j ? (int)j->pgid : 0);
        return out;
    }
    if (msh_streq(name, "-")) {
        char tmp[64];
        tmp[0] = '\0';
        if (msh_opt_nullglob)   strcat(tmp, "n");
        if (msh_opt_extglob)    strcat(tmp, "E");
        if (msh_opt_errexit)    strcat(tmp, "e");
        if (msh_opt_xtrace)     strcat(tmp, "x");
        snprintf(out, outsize, "%s", tmp);
        return out;
    }
    if (msh_streq(name, "#")) {
            int n = 0;
            for (int i = 0; i < 9; i++) if (st->pos_params[i]) n++;
            snprintf(out, outsize, "%d", n);
            return out;
        }
    if (name[0] >= '1' && name[0] <= '9' && name[1] == '\0') {
        int idx = name[0] - '0';
        const char *v = st->pos_params[idx - 1];
        if (!v) {
            /* try env MSHPARAMN */
            char vn[32];
            snprintf(vn, sizeof(vn), "MSHPARAM%d", idx);
            v = getenv(vn);
        }
        if (!v) v = "";
        snprintf(out, outsize, "%s", v);
        return out;
    }
    return NULL;
}

static char *cmdsub_eval(const char *cmd)
{
    int pipefd[2];
    if (pipe(pipefd) < 0) return msh_strdup("");
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]); close(pipefd[1]);
        return msh_strdup("");
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        msh_lexer_t lx;
        msh_lexer_init(&lx, cmd);
        if (msh_lexer_tokenize(&lx) == 0) {
            msh_ast_t *ast = msh_parse(&lx);
            if (ast) {
                int rc = msh_exec_ast(ast);
                (void)rc;
                msh_ast_free(ast);
            }
        }
        msh_lexer_free(&lx);
        _exit(0);
    }
    close(pipefd[1]);
    char buf[4096];
    size_t cap = 256, n = 0;
    char *result = msh_xmalloc(cap);
    ssize_t r;
    while ((r = read(pipefd[0], buf, sizeof(buf))) > 0) {
        if (n + (size_t)r + 1 >= cap) {
            cap = (n + (size_t)r + 1) * 2;
            result = msh_xrealloc(result, cap);
        }
        memcpy(result + n, buf, (size_t)r);
        n += (size_t)r;
    }
    result[n] = '\0';
    close(pipefd[0]);
    int status;
    waitpid(pid, &status, 0);
    while (n > 0 && result[n-1] == '\n') result[--n] = '\0';
    return result;
}

char *msh_expand_vars(const char *s)
{
    if (!s) return NULL;
    if (s[0] == '\'' && s[strlen(s)-1] == '\'') {
        size_t mid_len = strlen(s) - 2;
        if (!memchr(s + 1, '\'', mid_len)) {
            char *result = msh_xmalloc(mid_len + 1);
            memcpy(result, s + 1, mid_len);
            result[mid_len] = '\0';
            return result;
        }
    }

    char *buf = msh_xmalloc(strlen(s) * 2 + 16);
    size_t cap = strlen(s) * 2 + 16, n = 0;
    buf[0] = '\0';

    for (size_t i = 0; s[i]; ) {
        /* Arithmetic: $((expr)) */
        if (s[i] == '$' && s[i+1] == '(' && s[i+2] == '(') {
            const char *end = strstr(s + i + 3, "))");
            if (end) {
                size_t elen = (size_t)(end - (s + i + 3));
                char *expr = msh_strndup(s + i + 3, elen);
                long val = msh_arith_eval(expr);
                free(expr);
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%ld", val);
                append_str(&buf, &cap, &n, tmp);
                i = (size_t)(end - s) + 2;
                continue;
            }
        }

        /* Command substitution: $(cmd) */
        if (s[i] == '$' && s[i+1] == '(') {
            const char *p = s + i + 2;
            int depth = 1;
            while (*p && depth > 0) {
                if (*p == '(') depth++;
                else if (*p == ')') depth--;
                p++;
            }
            if (depth == 0) {
                size_t cmd_len = (size_t)(p - (s + i + 2)) - 1;
                char *cmd = msh_strndup(s + i + 2, cmd_len);
                char *out = cmdsub_eval(cmd);
                free(cmd);
                append_str(&buf, &cap, &n, out);
                free(out);
                i = (size_t)(p - s);
                continue;
            }
        }

        if (s[i] == '$' && s[i+1]) {
            char name[128]; int j = 0;
            i++;
            int bracketed = 0;
            if (s[i] == '{') {
                i++;
                bracketed = 1;
                while (s[i] && s[i] != '}' && j < 127) name[j++] = s[i++];
                if (s[i] == '}') i++;
            } else {
                while (s[i] && (msh_isalnum((unsigned char)s[i]) || s[i] == '_' || s[i] == '?' || s[i] == '$' || s[i] == '!' || s[i] == '-' || s[i] == '#' || (s[i] >= '0' && s[i] <= '9')) && j < 127)
                    name[j++] = s[i++];
            }
            name[j] = '\0';

            /* array access: arr[idx] or arr[@] */
            int is_array = 0;
            int arr_index = -1;
            char arr_var[128];
            if (bracketed) {
                /* parse name[idx] or name[@] */
                char *lb = strchr(name, '[');
                if (lb) {
                    *lb = '\0';
                    snprintf(arr_var, sizeof(arr_var), "%s", name);
                    char *idx = lb + 1;
                    char *rb = strchr(idx, ']');
                    if (rb) *rb = '\0';
                    if (msh_streq(idx, "@")) arr_index = -2; /* all */
                    else                      arr_index = atoi(idx);
                    is_array = 1;
                }
            } else {
                /* unbracketed: $name[i] ? not really — bash uses ${name[i]} */
            }

            char tmp[64];
            const char *v = NULL;
            if (is_array) {
                extern const char *msh_array_get(const char *name, int i);
                extern char *msh_array_join(const char *name);
                if (arr_index == -2) {
                    char *j = msh_array_join(arr_var);
                    append_str(&buf, &cap, &n, j);
                    free(j);
                } else {
                    v = msh_array_get(arr_var, arr_index);
                    if (v) append_str(&buf, &cap, &n, v);
                }
                continue;
            }
            v = special_var(name, tmp, sizeof(tmp));
            if (!v) v = getenv(name);
            if (v) append_str(&buf, &cap, &n, v);
            continue;
        }
        if (s[i] == '~' && (i == 0 || s[i-1] == ' ' || s[i-1] == ':' || s[i-1] == '=')) {
            const char *home = getenv("HOME");
            if (!home) home = "";
            append_str(&buf, &cap, &n, home);
            i++;
            continue;
        }
        append_chars(&buf, &cap, &n, &s[i], 1);
        i++;
    }
    return buf;
}

char *msh_expand_tilde(const char *s) { return msh_expand_vars(s); }

char *msh_expand_glob(const char *s, int *count, char ***out)
{
    wordexp_t we;
    if (wordexp(s, &we, WRDE_NOCMD) != 0) {
        if (count) *count = 1;
        *out = msh_xmalloc(sizeof(char*));
        (*out)[0] = msh_strdup(s);
        return (*out)[0];
    }
    if (count) *count = (int)we.we_wordc;
    *out = msh_xmalloc(sizeof(char*) * (we.we_wordc + 1));
    for (size_t i = 0; i < we.we_wordc; i++) (*out)[i] = msh_strdup(we.we_wordv[i]);
    (*out)[we.we_wordc] = NULL;
    char *first = (*out)[0] ? msh_strdup((*out)[0]) : NULL;
    wordfree(&we);
    return first;
}

/* ---- Arithmetic ---- */

static int arith_prec(const char **p)
{
    int prec = -1;
    while (**p == ' ') (*p)++;
    if (**p == '!') { prec = 4; (*p)++; }
    else if (**p == '~') { prec = 4; (*p)++; }
    else if (**p == '*' && (*p)[1] == '*') { prec = 3; *p += 2; }
    else if (**p == '*' || **p == '/' || **p == '%') { prec = 2; (*p)++; }
    else if (**p == '+' || **p == '-') { prec = 1; (*p)++; }
    else if ((**p == '<' || **p == '>') && (*p)[1] == **p) { prec = 0; *p += 2; } /* <<, >> */
    else if (**p == '<' || **p == '>') { prec = 0; (*p)++; }
    return prec;
}

static long arith_value(const char **p)
{
    while (**p == ' ') (*p)++;
    long val = 0;
    if (**p == '(') {
        (*p)++;
        val = msh_arith_eval(*p);
        while (**p && **p != ')') (*p)++;
        if (**p == ')') (*p)++;
        return val;
    }
    if (**p == '$' && (*p)[1] == '(' && (*p)[2] == '(') {
        /* nested $((...)) */
        const char *q = *p + 3;
        long v = msh_arith_eval(q);
        while (*q && (*q != ')' || q[1] != ')')) q++;
        *p = q + 2;
        return v;
    }
    if ((**p >= '0' && **p <= '9') || (**p == '-' && ((*p)[1] >= '0' && (*p)[1] <= '9'))) {
        char *end;
        val = strtol(*p, &end, 10);
        *p = end;
        return val;
    }
    if (msh_isalnum((unsigned char)**p) || **p == '_') {
        char name[128]; int j = 0;
        while ((msh_isalnum((unsigned char)**p) || **p == '_') && j < 127)
            name[j++] = *(*p)++;
        name[j] = '\0';
        const char *v = getenv(name);
        if (v) return atol(v);
        return 0;
    }
    return 0;
}

long msh_arith_eval(const char *expr)
{
    /* recursive-descent: primary; (op primary)* left-to-right at current precedence */
    /* We'll do a flat operator-precedence parser. */
    long left = arith_value(&expr);
    while (1) {
        while (*expr == ' ') expr++;
        if (!*expr || *expr == ')') break;
        /* check operator */
        const char *save = expr;
        int prec = arith_prec(&expr);
        if (prec < 0) break;
        long right = arith_value(&expr);
        /* apply with precedence */
        switch (prec) {
            case 1: /* + or - */
                if (*save == '+') left = left + right;
                else              left = left - right;
                break;
            case 2: /* * or / or % */
                if (*save == '*') left = left * right;
                else if (*save == '/') { if (right == 0) right = 1; left = left / right; }
                else                    { if (right == 0) right = 1; left = left % right; }
                break;
            case 3: /* ** */
                { long r = 1; long b = right < 0 ? -right : right;
                  while (b--) r *= left;
                  left = (right < 0 && left != 0 && (r < 0 || (left < 0 && right % 2))) ? -r : r; }
                break;
            case 4: /* ! or ~ */
                if (*save == '!') left = !right;
                else               left = ~right;
                break;
            case 0:
                if (save[0] == '<' && save[1] == '<')      left = ((unsigned long)left) << right;
                else if (save[0] == '>' && save[1] == '>') left = ((unsigned long)left) >> right;
                else if (*save == '<') left = left < right;
                else                    left = left > right;
                break;
        }
    }
    return left;
}