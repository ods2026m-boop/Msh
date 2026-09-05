#ifndef MSH_LEXER_H
#define MSH_LEXER_H

#include "msh.h"

typedef struct {
    const char    *src;
    int            pos;
    int            len;
    int            line;
    int            col;
    msh_token_t   *tokens;
    int            ntok;
    int            cap;
} msh_lexer_t;

void msh_lexer_init(msh_lexer_t *lx, const char *src);
void msh_lexer_free(msh_lexer_t *lx);
int  msh_lexer_tokenize(msh_lexer_t *lx); /* returns 0 on success */

#endif