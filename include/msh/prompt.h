#ifndef MSH_PROMPT_H
#define MSH_PROMPT_H

#include "msh.h"

void msh_prompt_init(void);
char *msh_prompt_get(void);
void msh_prompt_set(const char *p);
int  msh_prompt_input(char *buf, size_t max);  /* reads a line with editing/history */

#endif