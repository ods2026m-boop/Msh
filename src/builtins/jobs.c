/*
 * Msh - builtin: jobs
 */
#include "msh/builtin.h"
#include "msh/job.h"
#include "msh/util.h"

int msh_bi_jobs(int argc, char **argv)
{
    int only_running = 0, only_stopped = 0;
    for (int i = 1; i < argc; i++) {
        if (msh_streq(argv[i], "-l")) /* line-number, ignored for brevity */ ;
        else if (msh_streq(argv[i], "-r")) only_running = 1;
        else if (msh_streq(argv[i], "-s")) only_stopped = 1;
    }
    msh_job_update_status();
    msh_state_t *st = msh_get_state();
    for (msh_job_t *j = st->jobs_head; j; j = j->next) {
        if (only_running && j->status != JOB_RUNNING) continue;
        if (only_stopped && j->status != JOB_STOPPED) continue;
        msh_job_print(j);
    }
    fflush(stdout);
    return 0;
}