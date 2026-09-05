/*
 * Msh - Lexer
 * Converts a command-line string into tokens.
 * Supports: words, single/double-quoted strings, redirections,
 *           pipes, backgrounding, and logical operators.
 */
#include "msh/lexer.h"
#include "msh/util.h"

void msh_lexer_init(msh_lexer_t *lx, const char *src)
{
    memset(lx, 0, sizeof(*lx));
    lx->src = src;
    lx->len = (int)strlen(src);
    lx->line = 1;
    lx->col = 1;
}

void msh_lexer_free(msh_lexer_t *lx)
{
    if (!lx) return;
    for (int i = 0; i < lx->ntok; i++) free(lx->tokens[i].value);
    free(lx->tokens);
    memset(lx, 0, sizeof(*lx));
}

static void lx_push(msh_lexer_t *lx, msh_token_type_t t, const char *v, int line, int col)
{
    if (lx->ntok >= lx->cap) {
        lx->cap = lx->cap ? lx->cap * 2 : 16;
        lx->tokens = msh_xrealloc(lx->tokens, lx->cap * sizeof(msh_token_t));
    }
    lx->tokens[lx->ntok].type = t;
    lx->tokens[lx->ntok].value = v ? msh_strdup(v) : NULL;
    lx->tokens[lx->ntok].line = line;
    lx->tokens[lx->ntok].col  = col;
    lx->ntok++;
}

static int lx_peek(msh_lexer_t *lx) { return lx->pos < lx->len ? (unsigned char)lx->src[lx->pos] : -1; }
static int lx_advance(msh_lexer_t *lx) __attribute__((unused));
static int lx_advance(msh_lexer_t *lx)
{
    int c = lx_peek(lx);
    if (c == '\n') { lx->line++; lx->col = 1; }
    else lx->col++;
    lx->pos++;
    return c;
}
static void lx_skip_ws(msh_lexer_t *lx)
{
    while (lx->pos < lx->len) {
        int c = (unsigned char)lx->src[lx->pos];
        if (c == ' ' || c == '\t' || c == '\r') { lx->pos++; lx->col++; }
        else if (c == '\\' && lx->pos + 1 < lx->len && lx->src[lx->pos+1] == '\n') {
            lx->pos += 2; lx->line++; lx->col = 1;
        }
        else if (c == '\n') {
            lx->pos++; lx->line++; lx->col = 1;
            lx_push(lx, TOK_SEMI, NULL, lx->line, lx->col);
        }
        else break;
    }
}

static int read_word(msh_lexer_t *lx, char *out, size_t max, int *quoted, char *qchar)
{
    size_t n = 0;
    *quoted = 0;
    if (qchar) *qchar = 0;
    while (lx->pos < lx->len && n + 1 < max) {
        int c = (unsigned char)lx->src[lx->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') break;
        if (c == '|' || c == '&' || c == ';' || c == '<' || c == '>' ) break;
        if (c == '(' || c == ')') {
            if (n > 0) break;
            /* allow () as standalone tokens but not inside words */
            out[n++] = (char)c;
            lx->pos++; lx->col++;
            out[n] = '\0';
            return (int)n;
        }
        if (c == '"' || c == '\'') {
            char q = c;
            *quoted = 1;
            if (qchar) *qchar = q;
            lx->pos++; lx->col++;
            while (lx->pos < lx->len && lx->src[lx->pos] != q) {
                if (q == '"' && lx->src[lx->pos] == '\\' && lx->pos + 1 < lx->len) {
                    int nxt = (unsigned char)lx->src[lx->pos+1];
                    if (nxt == '\\' || nxt == '"' || nxt == '$' || nxt == '`' || nxt == '\n') {
                        if (nxt == '\n') { lx->pos++; lx->line++; lx->col = 1; continue; }
                        if (n + 1 < max) out[n++] = (char)nxt;
                        lx->pos += 2; lx->col += 2;
                        continue;
                    }
                }
                if (n + 1 < max) out[n++] = lx->src[lx->pos];
                lx->pos++; lx->col++;
            }
            if (lx->pos < lx->len) { /* consume the closing quote */
                lx->pos++; lx->col++;
            }
            break;
        }
        if (n + 1 < max) out[n++] = lx->src[lx->pos];
        lx->pos++; lx->col++;
    }
    out[n] = '\0';
    return (int)n;
}

int msh_lexer_tokenize(msh_lexer_t *lx)
{
    if (msh_get_state()->debug) {
        fprintf(stderr, "LEXER_TOKENIZE START\n");
        fflush(stderr);
    }
    while (1) {
        lx_skip_ws(lx);
        if (lx->pos >= lx->len) {
            if (msh_get_state()->debug) {
                 fprintf(stderr, "LEXER: END OF INPUT, pos=%d, len=%d, line='%s'\n", lx->pos, lx->len, lx->src);
                 fprintf(stderr, "TOKENS: ");
                 for (int i = 0; i < lx->ntok; i++) {
                     fprintf(stderr, "[%d:%s] ", lx->tokens[i].type, lx->tokens[i].value ? lx->tokens[i].value : "(null)");
                 }
                 fprintf(stderr, "\n");
                 fflush(stderr);
             }
            lx_push(lx, TOK_EOF, NULL, lx->line, lx->col);
            return 0;
        }
        int c = (unsigned char)lx->src[lx->pos];
        int line = lx->line, col = lx->col;

        if (c == '$' && lx->pos + 2 < lx->len && lx->src[lx->pos+1] == '(' && lx->src[lx->pos+2] == '(') {
            int start = lx->pos;
            int depth = 2; /* $(( contributes two opening parens */
            lx->pos += 3; lx->col += 3;
            while (lx->pos < lx->len && depth > 0) {
                if (lx->src[lx->pos] == '(') { depth++; lx->pos++; lx->col++; }
                else if (lx->src[lx->pos] == ')') {
                    depth--;
                    if (depth == 0) { lx->pos++; lx->col++; break; }
                    lx->pos++; lx->col++;
                } else {
                    lx->pos++; lx->col++;
                }
            }
            char *buf = msh_strndup(lx->src + start, lx->pos - start);
            lx_push(lx, TOK_WORD, buf, line, col);
            free(buf);
            continue;
        }
        if (c == '$' && lx->pos + 1 < lx->len && lx->src[lx->pos+1] == '(') {
            int start = lx->pos;
            int depth = 1; /* $( contributes one opening paren */
            lx->pos += 2; lx->col += 2;
            while (lx->pos < lx->len && depth > 0) {
                if (lx->src[lx->pos] == '(') { depth++; lx->pos++; lx->col++; }
                else if (lx->src[lx->pos] == ')') {
                    depth--;
                    if (depth == 0) { lx->pos++; lx->col++; break; }
                    lx->pos++; lx->col++;
                } else {
                    lx->pos++; lx->col++;
                }
            }
            char *buf = msh_strndup(lx->src + start, lx->pos - start);
            lx_push(lx, TOK_CMDSUB, buf, line, col);
            free(buf);
            continue;
        }

        if (c == '|') {
            lx->pos++; lx->col++;
            if (lx_peek(lx) == '|') { lx->pos++; lx->col++; lx_push(lx, TOK_OR, "||", line, col); }
            else lx_push(lx, TOK_PIPE, "|", line, col);
            continue;
        }
        if (c == '&' && lx->pos + 1 < lx->len && lx->src[lx->pos+1] == '>') {
            lx->pos += 2; lx->col += 2;
            if (lx_peek(lx) == '>') { lx->pos++; lx->col++; lx_push(lx, TOK_REDIR_BOTH_APP, "&>>", line, col); }
            else lx_push(lx, TOK_REDIR_BOTH, "&>", line, col);
            continue;
        }
        if (c == '&') {
            lx->pos++; lx->col++;
            if (lx_peek(lx) == '&') { lx->pos++; lx->col++; lx_push(lx, TOK_AND, "&&", line, col); }
            else lx_push(lx, TOK_BG, "&", line, col);
            continue;
        }
        if (c == ';') { lx->pos++; lx->col++; lx_push(lx, TOK_SEMI, ";", line, col); continue; }
        if (c == '(') { lx->pos++; lx->col++; lx_push(lx, TOK_LPAREN, "(", line, col); continue; }
        if (c == ')') { lx->pos++; lx->col++; lx_push(lx, TOK_RPAREN, ")", line, col); continue; }
        if (c == '>') {
            lx->pos++; lx->col++;
            if (lx_peek(lx) == '>') { lx->pos++; lx->col++; lx_push(lx, TOK_REDIR_APP, ">>", line, col); }
            else lx_push(lx, TOK_REDIR_OUT, ">", line, col);
            continue;
        }
        if (c == '<' && lx->pos + 1 < lx->len && lx->src[lx->pos+1] == '<') {
            lx->pos += 2; lx->col += 2;
            lx_push(lx, TOK_REDIR_HEREDOC, "<<", line, col);
            continue;
        }
        if (c == '<') {
            lx->pos++; lx->col++;
            lx_push(lx, TOK_REDIR_IN, "<", line, col);
            continue;
        }
        if (c == '{') { lx->pos++; lx->col++; lx_push(lx, TOK_LBRACE, "{", line, col); continue; }
        if (c == '}') { lx->pos++; lx->col++; lx_push(lx, TOK_RBRACE, "}", line, col); continue; }
        if (c == '2' && lx->pos + 1 < lx->len && lx->src[lx->pos+1] == '>') {
            lx->pos += 2; lx->col += 2;
            if (lx_peek(lx) == '>') { lx->pos++; lx->col++; lx_push(lx, TOK_REDIR_ERR, "2>>", line, col); }
            else lx_push(lx, TOK_REDIR_ERR, "2>", line, col);
            continue;
        }
        /* word or quoted string */
        char buf[MSH_MAX_LINE];
        int quoted;
        char qchar = 0;
        int n = read_word(lx, buf, sizeof(buf), &quoted, &qchar);
        if (n == 0) {
            lx->pos++; lx->col++;
            continue;
        }
        msh_token_type_t kw = TOK_WORD;
        if (!quoted) {
            if      (msh_streq(buf, "if"))       kw = TOK_IF;
            else if (msh_streq(buf, "then"))     kw = TOK_THEN;
            else if (msh_streq(buf, "else"))     kw = TOK_ELSE;
            else if (msh_streq(buf, "fi"))       kw = TOK_FI;
            else if (msh_streq(buf, "while"))    kw = TOK_WHILE;
            else if (msh_streq(buf, "do"))       kw = TOK_DO;
            else if (msh_streq(buf, "done"))     kw = TOK_DONE;
            else if (msh_streq(buf, "for"))      kw = TOK_FOR;
            else if (msh_streq(buf, "in"))       kw = TOK_IN;
            else if (msh_streq(buf, "function")) kw = TOK_FUNCTION;
        }
        if (quoted && qchar == '\'') {
            char marked[MSH_MAX_LINE + 3];
            marked[0] = '\'';
            size_t len = (size_t)n;
            if (len >= MSH_MAX_LINE) len = MSH_MAX_LINE - 1;
            memcpy(marked + 1, buf, len);
            marked[len + 1] = '\'';
            marked[len + 2] = '\0';
            lx_push(lx, TOK_STRING, marked, line, col);
        } else {
            lx_push(lx, quoted ? TOK_STRING : kw, buf, line, col);
        }
    }
}