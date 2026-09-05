#ifndef MSH_JOB_H
#define MSH_JOB_H

#include "msh.h"

void msh_job_init(void);
msh_job_t *msh_job_add(pid_t pgid, const char *cmdline);
void msh_job_remove(msh_job_t *j);
msh_job_t *msh_job_find(int id);
msh_job_t *msh_job_find_by_pgid(pid_t pgid);
msh_job_t *msh_job_find_by_prefix(const char *prefix);
msh_job_t *msh_job_last_or_current(void);
msh_job_t *msh_job_previous(void);
void msh_job_update_status(void);
void msh_job_print(msh_job_t *j);
void msh_job_print_all(void);
int  msh_job_wait(msh_job_t *j);

#endif