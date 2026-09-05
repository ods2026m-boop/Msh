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

