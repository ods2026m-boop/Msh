#ifndef MSH_FUNCTION_H
#define MSH_FUNCTION_H

#include "msh.h"

void        msh_function_define(const char *name, const char *body);
const char *msh_function_lookup(const char *name);
int         msh_function_call(const char *name, int argc, char **argv);
void        msh_function_clear_all(void);

#endif