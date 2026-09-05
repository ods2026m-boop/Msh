#ifndef MSH_EXPAND_H
#define MSH_EXPAND_H

#include "msh.h"

char *msh_expand_vars(const char *s);
char *msh_expand_tilde(const char *s);
char *msh_expand_glob(const char *s, int *count, char ***out);
long  msh_arith_eval(const char *expr);

#endif