#ifndef MSH_COMPLETION_H
#define MSH_COMPLETION_H

#include "msh.h"

typedef struct {
    char **items;
    int    count;
    int    cap;
} msh_completions_t;

void msh_completion_init(void);
void msh_completion_free(msh_completions_t *c);

/* Compute completions for the partial input (line up to cursor). */
void msh_complete(const char *line, int cursor, msh_completions_t *out);
char *msh_completion_common_prefix(msh_completions_t *c);

#endif