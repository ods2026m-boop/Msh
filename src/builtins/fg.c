/*
 * Msh - builtin: fg
 * Brings a job to the foreground.
 * Syntax: fg [%job_id] — supports %1, %+, %-, %string prefix.
 */
#include "msh/builtin.h"
#include "msh/job.h"
#include "msh/util.h"
#include <termios.h>

static msh_job_t *resolve_job(const char *spec)
{
    if (!spec) {
        return msh_job_last_or_current();
    }
    if (spec[0] == '%') spec++;
    if (!*spec || msh_streq(spec, "+"))
        return msh_job_last_or_current();
    if (msh_streq(spec, "-"))
        return msh_job_previous();
    if (msh_isdigit((unsigned char)spec[0])) {
        return msh_job_find(atoi(spec));
    }
    /* prefix match on cmdline */
    return msh_job_find_by_prefix(spec);
}

int msh_bi_fg(int argc, char **argv)
{
    const char *spec = argc > 1 ? argv[1] : NULL;
    msh_job_t *j = resolve_job(spec);
    if (!j) { msh_error("fg: %s: no such job", spec ? spec : "current"); return 1; }

    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, j->pgid);
    if (j->status == JOB_STOPPED) {
        kill(-j->pgid, SIGCONT);
        j->status = JOB_RUNNING;
    }
    int rc = msh_job_wait(j);
    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, getpgrp());
    return rc;
}