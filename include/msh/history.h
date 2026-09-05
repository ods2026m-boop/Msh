#ifndef MSH_HISTORY_H
#define MSH_HISTORY_H

#include "msh.h"

void  msh_history_init(void);
void  msh_history_add(const char *line);
char *msh_history_get(int index);
int   msh_history_size(void);
void  msh_history_clear(void);
void  msh_history_delete(int index);
const char *msh_history_prev(const char *current);
const char *msh_history_next(const char *current);
void  msh_history_load(const char *path);
void  msh_history_save(const char *path);

#endif