#ifndef MSH_PARSER_H
#define MSH_PARSER_H

#include "msh.h"
#include "msh/lexer.h"

msh_ast_t *msh_parse(msh_lexer_t *lx);
void       msh_ast_free(msh_ast_t *ast);
void       msh_redir_free(msh_redir_t *r);
void       msh_cmd_free(msh_cmd_t *c);
void       msh_pipeline_free(msh_pipeline_t *p);

#endif