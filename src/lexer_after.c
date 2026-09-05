
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
        int n = read_word(lx, buf, sizeof(buf), &quoted);
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
        lx_push(lx, quoted ? TOK_STRING : kw, buf, line, col);
    }
}