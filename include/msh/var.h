#ifndef MSH_VAR_H
#define MSH_VAR_H

#include "msh.h"

char *msh_var_get(const char *name);
int   msh_var_set(const char *name, const char *value);
int   msh_var_unset(const char *name);
char **msh_var_list(int *count);
int   msh_var_export(const char *name);

#endif