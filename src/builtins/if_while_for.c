/*
 * Msh - compound command builtins (if/while/for/{...}).
 *
 * When the parser/exec encounters an `if`/`while`/`for`/`{` at the start
 * of a line, it routes to msh_compound_dispatch() which reads the rest
 * of the compound from the input source.
 */
#include "msh/builtin.h"
#include "msh/util.h"
#include "msh/compound.h"
#include "msh/lexer.h"
#include "msh/parser.h"
#include "msh/exec.h"
#include <unistd.h>

/* execute a single line and return its status */
static int eval_line(const char *line)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, line);
    if (msh_lexer_tokenize(&lx) != 0) { msh_lexer_free(&lx); return 0; }
    msh_ast_t *ast = msh_parse(&lx);
    int rc = 0;
    if (ast) { rc = msh_exec_ast(ast); msh_ast_free(ast); }
    msh_lexer_free(&lx);
    return rc;
}

static const char *skip_to_kw(const char *body, const char *kw)
{
    size_t l = strlen(kw);
    const char *p = body;
    while (*p) {
        if (strncmp(p, kw, l) == 0) {
            char next = p[l];
            if (next == ' ' || next == '\t' || next == '\n' || next == ';' || next == '\0')
                return p;
        }
        p++;
    }
    return NULL;
}

/* Find a top-level `fi` / `done` / `}` respecting nested if/while. */
static int find_terminator(const char *body, const char *term)
{
    size_t l = strlen(term);
    const char *p = body;
    int depth = 0;
    while (*p) {
        if (strncmp(p, "if", 2) == 0 && (p[2] == ' ' || p[2] == '\n')) depth++;
        else if (strncmp(p, term, l) == 0 &&
                 (p[l] == ' ' || p[l] == '\n' || p[l] == ';' || p[l] == '\0')) {
            /* ensure not preceded by word char (so "done" in "echo done" doesn't match) */
            if (p > body) {
                char prev = p[-1];
                if (prev != ' ' && prev != '\t' && prev != '\n' && prev != ';' && prev != '|' && prev != '&') continue;
            }
            if (strncmp(term, "fi", 2) == 0 && depth > 0) { depth--; }
            else return (int)(p - body);
        }
        p++;
    }
    return -1;
}

static char *substrdup(const char *s, size_t start, size_t end)
{
    if (end < start) end = start;
    return msh_strndup(s + start, end - start);
}

static int exec_if(const char *body)
{
    const char *then_pos = skip_to_kw(body, "then");
    if (!then_pos) { msh_error("if: missing 'then'"); return 1; }
    char *cond = substrdup(body, 0, then_pos - body);
    int rc = eval_line(cond);
    free(cond);
    const char *rest = then_pos + 4;
    while (*rest == ' ' || *rest == '\t' || *rest == ';') rest++;
    int fi_pos = find_terminator(rest, "fi");
    if (fi_pos < 0) { msh_error("if: missing 'fi'"); return 1; }
    int else_pos = -1;
    {
        const char *p = rest; int depth = 0;
        while (p < rest + fi_pos) {
            if (strncmp(p, "if", 2) == 0 && (p[2] == ' ' || p[2] == '\n')) depth++;
            else if (strncmp(p, "fi", 2) == 0 && (p[2] == ' ' || p[2] == '\n' || p[2] == ';')) { if (depth > 0) depth--; }
            else if (depth == 0 && strncmp(p, "else", 4) == 0 &&
                     (p[4] == ' ' || p[4] == '\n' || p[4] == ';')) {
                else_pos = (int)(p - rest); break;
            }
            p++;
        }
    }
    char *then_body = NULL, *else_body = NULL;
    if (else_pos >= 0) {
        then_body = substrdup(rest, 0, else_pos);
        else_body = substrdup(rest, else_pos + 4, fi_pos);
    } else {
        then_body = substrdup(rest, 0, fi_pos);
    }
    int result = 0;
    if (rc == 0) result = msh_compound_exec(then_body);
    else if (else_body) result = msh_compound_exec(else_body);
    else result = rc;
    free(then_body); free(else_body);
    return result;
}

static int exec_while(const char *body)
{
    const char *do_pos = skip_to_kw(body, "do");
    if (!do_pos) { msh_error("while: missing 'do'"); return 1; }
    char *cond = substrdup(body, 0, do_pos - body);
    const char *rest = do_pos + 2;
    while (*rest == ' ' || *rest == '\t' || *rest == ';') rest++;
    int done_pos = find_terminator(rest, "done");
    if (done_pos < 0) { msh_error("while: missing 'done'"); free(cond); return 1; }
    char *loop_body = substrdup(rest, 0, done_pos);

    int last = 0;
    int max_iter = 100000;
    while (max_iter-- > 0) {
        int c = eval_line(cond);
        if (c != 0) break;
        last = msh_compound_exec(loop_body);
    }
    free(cond); free(loop_body);
    return last;
}

static int exec_for(const char *body)
{
    const char *in_pos = skip_to_kw(body, "in");
    if (!in_pos) { msh_error("for: missing 'in'"); return 1; }
    const char *p = body;
    while (*p == ' ' || *p == '\t') p++;
    const char *var_start = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    char *var = substrdup(var_start, 0, p - var_start);

    const char *rest = in_pos + 2;
    while (*rest == ' ' || *rest == '\t' || *rest == ';') rest++;
    const char *do_pos = skip_to_kw(rest, "do");
    if (!do_pos) { msh_error("for: missing 'do'"); free(var); return 1; }
    char *words_str = substrdup(rest, 0, do_pos - rest);

    const char *after_do = do_pos + 2;
    while (*after_do == ' ' || *after_do == '\t' || *after_do == ';') after_do++;
    int done_pos = find_terminator(after_do, "done");
    if (done_pos < 0) { msh_error("for: missing 'done'"); free(var); free(words_str); return 1; }
    char *loop_body = substrdup(after_do, 0, done_pos);

    int wc = 0;
    char **words = msh_strsplit(words_str, " \t\n", &wc);
    int last = 0;
    for (int i = 0; i < wc; i++) {
        if (!words[i][0]) continue;
        /* strip trailing ; */
        size_t l = strlen(words[i]);
        while (l && words[i][l-1] == ';') words[i][--l] = '\0';
        if (!l) continue;
        setenv(var, words[i], 1);
        last = msh_compound_exec(loop_body);
    }
    free(var); free(words_str); free(loop_body);
    msh_strfreev(words);
    return last;
}

static int exec_brace_block(const char *body)
{
    int depth = 1;
    const char *p = body;
    while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') { depth--; if (depth == 0) break; }
        p++;
    }
    if (depth != 0) { msh_error("missing closing }"); return 1; }
    return msh_compound_exec(substrdup(body, 0, p - body));
}

int msh_compound_dispatch(const char *keyword, const char *body)
{
    if (msh_streq(keyword, "if"))    return exec_if(body);
    if (msh_streq(keyword, "while")) return exec_while(body);
    if (msh_streq(keyword, "for"))   return exec_for(body);
    if (msh_streq(keyword, "{"))     return exec_brace_block(body);
    return 1;
}

/* Read multi-line compound from fd until terminator is seen (and consumed). */
char *msh_read_compound_to_terminator(int fd, const char *terminator)
{
    size_t cap = 512, n = 0;
    char *buf = msh_xmalloc(cap);
    char line[MSH_MAX_LINE];
    while (1) {
        ssize_t r = msh_read_line(fd, line, sizeof(line));
        if (r <= 0) break;
        char *trim = msh_strtrim(line);
        if (msh_streq(trim, terminator)) break;
        size_t l = strlen(line);
        if (n + l + 2 >= cap) { cap = (n + l + 2) * 2; buf = msh_xrealloc(buf, cap); }
        memcpy(buf + n, line, l); n += l;
        buf[n++] = '\n'; buf[n] = '\0';
    }
    return buf;
}