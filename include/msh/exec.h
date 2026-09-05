#ifndef MSH_EXEC_H
#define MSH_EXEC_H

#include "msh.h"

int  msh_exec_ast(msh_ast_t *ast);
int  msh_exec_pipeline(msh_pipeline_t *pl, int background);
int  msh_apply_redirs(msh_redir_t *r);
char *msh_resolve_path(const char *cmd);
int  msh_is_builtin(const char *cmd);
int  msh_run_builtin(int argc, char **argv);

#endif