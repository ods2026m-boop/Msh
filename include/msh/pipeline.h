#ifndef MSH_PIPELINE_H
#define MSH_PIPELINE_H

#include "msh.h"

/* Execute a pipeline of commands connected via pipes.
 * The first command reads from stdin; the last writes to stdout.
 * Intermediate commands are connected via pipe() pairs.
 * Returns the exit status of the last command, or 0 if background.
 */
int msh_pipeline_run(msh_pipeline_t *pl, int background);

/* Build a human-readable representation of a pipeline. */
char *msh_pipeline_to_string(msh_pipeline_t *pl);

#endif