/*
 * Msh - Compound command execution (if/while/for/{...}).
 *
 * Compound commands in Msh use a simple multi-line syntax:
 *   if COND; then BODY; else BODY; fi
 *   while COND; do BODY; done
 *   for VAR in WORDS; do BODY; done
 *   { BODY; }
 *
 * Lexer tokens for these keywords are added to the token types.
 */
#include "msh/compound.h"
#include "msh/util.h"
#include "msh/lexer.h"
#include "msh/parser.h"
#include "msh/exec.h"
#include <unistd.h>

int msh_compound_exec(const char *body)
{
    if (!body) return 0;
    char *line = msh_strdup(body);
    char *trim = msh_strtrim(line);
    if (!*trim || *trim == '#') { free(line); return 0; }
    msh_lexer_t lx;
    msh_lexer_init(&lx, trim);
    int rc = 0;
    if (msh_lexer_tokenize(&lx) == 0) {
        msh_ast_t *ast = msh_parse(&lx);
        if (ast) {
            rc = msh_exec_ast(ast);
            msh_get_state()->last_status = rc;
            msh_ast_free(ast);
        }
    }
    msh_lexer_free(&lx);
    free(line);
    return rc;
}

static int match_terminator(const char *trim, const char *term)
{
    if (!term) return 0;
    size_t l = strlen(term);
    if (strncmp(trim, term, l) == 0) {
        char next = trim[l];
        return next == '\0' || next == ' ' || next == '\t' || next == ';' || next == '\n';
    }
    /* for nested if/while, allow "fi" as terminator only at outer level.
     * For simplicity, we treat matching word as terminator if followed by
     * whitespace or punctuation. */
    if (msh_streq(trim, term)) return 1;
    return 0;
}

char *msh_read_compound(int fd, const char *terminator)
{
    size_t cap = 256, n = 0;
    char *buf = msh_xmalloc(cap);
    char line[MSH_MAX_LINE];
    while (1) {
        ssize_t r = msh_read_line(fd, line, sizeof(line));
        if (r < 0) break;
        char *trim = msh_strtrim(line);
        if (terminator && match_terminator(trim, terminator)) break;
        size_t l = strlen(line);
        if (n + l + 2 >= cap) { cap = (n + l + 2) * 2; buf = msh_xrealloc(buf, cap); }
        memcpy(buf + n, line, l); n += l;
        buf[n++] = '\n'; buf[n] = '\0';
    }
    return buf;
}