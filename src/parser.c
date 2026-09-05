/*
 * Msh - Parser
 * Builds a simple AST from tokens: list of pipelines, each pipeline a list of commands.
 * Supports: pipes, redirections, background (&), lists separated by ; / && / ||.
 */
#include "msh/parser.h"
#include "msh/util.h"

void msh_redir_free(msh_redir_t *r)
{
    while (r) {
        msh_redir_t *n = r->next;
        free(r->target);
        free(r);
        r = n;
    }
}

void msh_cmd_free(msh_cmd_t *c)
{
    if (!c) return;
    if (c->argv) {
        for (int i = 0; i < c->argc; i++) free(c->argv[i]);
        free(c->argv);
    }
    msh_redir_free(c->redirs);
    /* poison to catch use-after-free */
    if (sizeof(*c) >= sizeof(void*)) {
        /* don't actually poison - keep simple */
    }
}

void msh_pipeline_free(msh_pipeline_t *p)
{
    while (p) {
        msh_pipeline_t *n = p->next;
        for (int i = 0; i < p->ncmds; i++) msh_cmd_free(&p->cmds[i]);
        free(p->cmds);
        free(p);
        p = n;
    }
}

void msh_ast_free(msh_ast_t *ast)
{
    if (!ast) return;
    msh_pipeline_free(ast->head);
    free(ast);
}

static int peek(msh_lexer_t *lx) __attribute__((unused));
static int peek(msh_lexer_t *lx) { return lx->ntok > 0 ? lx->tokens[lx->ntok-1].type : TOK_EOF; }

typedef struct {
    msh_lexer_t *lx;
    int          pos;
    int          err;
} pst_t;

static msh_token_t *cur(pst_t *p) { return &p->lx->tokens[p->pos]; }
static void next_tok(pst_t *p) { if (p->pos < p->lx->ntok - 1) p->pos++; }

static msh_redir_type_t redir_kind(msh_token_type_t t)
{
    switch (t) {
        case TOK_REDIR_IN:        return REDIR_IN;
        case TOK_REDIR_OUT:       return REDIR_OUT;
        case TOK_REDIR_APP:       return REDIR_APPEND;
        case TOK_REDIR_ERR:       return REDIR_ERR;
        case TOK_REDIR_BOTH:      return REDIR_BOTH;
        case TOK_REDIR_BOTH_APP:  return REDIR_BOTH_APP;
        case TOK_REDIR_HEREDOC:   return REDIR_HEREDOC;
        default:                  return REDIR_NONE;
    }
}

/* parse one simple command, possibly with redirections. */
static msh_cmd_t *parse_command(pst_t *p, msh_cmd_t *cmd)
{
    memset(cmd, 0, sizeof(*cmd));
    int cap = 8;
    cmd->argv = msh_xmalloc(sizeof(char*) * cap);
    cmd->argc = 0;

    while (1) {
        msh_token_t *t = cur(p);
        if (t->type == TOK_WORD || t->type == TOK_STRING || t->type == TOK_CMDSUB ||
            t->type == TOK_IF || t->type == TOK_THEN || t->type == TOK_ELSE || t->type == TOK_FI ||
            t->type == TOK_WHILE || t->type == TOK_DO || t->type == TOK_DONE ||
            t->type == TOK_FOR || t->type == TOK_IN || t->type == TOK_FUNCTION ||
            t->type == TOK_LPAREN || t->type == TOK_RPAREN ||
            t->type == TOK_LBRACE || t->type == TOK_RBRACE) {
            if (cmd->argc + 1 >= cap) {
                cap *= 2;
                cmd->argv = msh_xrealloc(cmd->argv, sizeof(char*) * cap);
            }
            cmd->argv[cmd->argc++] = msh_strdup(t->value);
            cmd->argv[cmd->argc] = NULL;
            next_tok(p);
            continue;
        }
        if (t->type == TOK_REDIR_IN || t->type == TOK_REDIR_OUT ||
            t->type == TOK_REDIR_APP || t->type == TOK_REDIR_ERR ||
            t->type == TOK_REDIR_BOTH || t->type == TOK_REDIR_BOTH_APP ||
            t->type == TOK_REDIR_HEREDOC) {
            msh_redir_type_t kind = redir_kind(t->type);
            next_tok(p);
            msh_token_t *tgt = cur(p);
            if (tgt->type != TOK_WORD && tgt->type != TOK_STRING) {
                msh_error("expected filename after redirection");
                p->err = 1;
                return NULL;
            }
            msh_redir_t *r = msh_xcalloc(1, sizeof(*r));
            r->type = kind;
            r->target = msh_strdup(tgt->value);
            r->fd = (kind == REDIR_ERR) ? 2 : 1;
            r->next = cmd->redirs;
            cmd->redirs = r;
            next_tok(p);
            continue;
        }
        break;
    }
    if (cmd->argc == 0 && !cmd->redirs) { free(cmd->argv); return NULL; }
    return cmd;
}

static msh_pipeline_t *parse_pipeline(pst_t *p)
{
    int cap = 4, n = 0;
    msh_cmd_t *cmds = msh_xmalloc(sizeof(msh_cmd_t) * cap);
    while (1) {
        msh_cmd_t cmd;
        if (!parse_command(p, &cmd)) break;
        if (n >= cap) { cap *= 2; cmds = msh_xrealloc(cmds, sizeof(msh_cmd_t) * cap); }
        cmds[n++] = cmd;
        if (cur(p)->type != TOK_PIPE) break;
        next_tok(p);
    }
    if (n == 0) { free(cmds); return NULL; }
    msh_pipeline_t *pl = msh_xcalloc(1, sizeof(*pl));
    pl->cmds = cmds;
    pl->ncmds = n;
    return pl;
}

static msh_pipeline_t *parse_pipeline_list(pst_t *p)
{
    msh_pipeline_t *head = NULL, *tail = NULL;
    while (1) {
        msh_pipeline_t *pl = parse_pipeline(p);
        if (!pl) break;
        if (!head) head = tail = pl;
        else { tail->next = pl; tail = pl; }
        if (cur(p)->type == TOK_SEMI || cur(p)->type == TOK_AND ||
            cur(p)->type == TOK_OR || cur(p)->type == TOK_BG ||
            cur(p)->type == TOK_EOF) {
            if (cur(p)->type == TOK_BG) tail->background = 1;
            if (cur(p)->type != TOK_EOF) next_tok(p);
            continue;
        }
        break;
    }
    return head;
}

msh_ast_t *msh_parse(msh_lexer_t *lx)
{
    pst_t p = { .lx = lx, .pos = 0, .err = 0 };
    msh_ast_t *ast = msh_xcalloc(1, sizeof(*ast));
    ast->head = parse_pipeline_list(&p);
    if (cur(&p)->type == TOK_BG) {
        ast->background = 1;
        next_tok(&p);
    }
    if (p.err) { msh_ast_free(ast); return NULL; }
    return ast;
}