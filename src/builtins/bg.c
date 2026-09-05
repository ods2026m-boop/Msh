/*
 * Msh - builtin: bg
 * Resumes a stopped job in the background.
 */
#include "msh/builtin.h"
#include "msh/job.h"
#include "msh/util.h"

static msh_job_t *resolve_job(const char *spec)
{
    if (!spec) return msh_job_last_or_current();
    if (spec[0] == '%') spec++;
    if (!*spec || msh_streq(spec, "+")) return msh_job_last_or_current();
    if (msh_streq(spec, "-")) return msh_job_previous();
    if (msh_isdigit((unsigned char)spec[0])) return msh_job_find(atoi(spec));
    return msh_job_find_by_prefix(spec);
}

int msh_bi_bg(int argc, char **argv)
{
    const char *spec = argc > 1 ? argv[1] : NULL;
    msh_job_t *j = resolve_job(spec);
    if (!j) { msh_error("bg: %s: no such job", spec ? spec : "current"); return 1; }
    kill(-j->pgid, SIGCONT);
    j->status = JOB_RUNNING;
    fprintf(stderr, "[%d] %s &\n", j->id, j->cmdline);
    return 0;
}