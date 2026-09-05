#ifndef MSH_COMPOUND_H
#define MSH_COMPOUND_H

#include "msh.h"

int  msh_compound_exec(const char *body);
int  msh_compound_dispatch(const char *keyword, const char *body);
char *msh_read_compound_to_terminator(int fd, const char *terminator);

#endif