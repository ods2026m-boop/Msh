#ifndef MSH_ARRAY_H
#define MSH_ARRAY_H

#include "msh.h"

void        msh_array_set(const char *name, int index, const char *value);
const char *msh_array_get(const char *name, int index);
char       *msh_array_join(const char *name);
int         msh_array_count(const char *name);
void        msh_array_clear(const char *name);
void        msh_array_handle_assignment(const char *assignment);
void        msh_array_cleanup_all(void);

#endif