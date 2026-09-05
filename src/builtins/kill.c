/*
 * Msh - builtin: kill
 * Send signal to a job (%n) or process (pid).
 */
#include "msh/builtin.h"
#include "msh/job.h"
#include "msh/util.h"
#include <signal.h>
#include <string.h>

static int parse_signal(const char *s, int *signo)
{
    if (!s || !s[0]) return 0;
    if (s[0] != '-') return 0;
    if (msh_streq(s, "-TERM") || msh_streq(s, "-15")) { *signo = SIGTERM; return 1; }
    if (msh_streq(s, "-KILL") || msh_streq(s, "-9"))  { *signo = SIGKILL; return 1; }
    if (msh_streq(s, "-INT")  || msh_streq(s, "-2"))   { *signo = SIGINT;  return 1; }
    if (msh_streq(s, "-HUP")  || msh_streq(s, "-1"))   { *signo = SIGHUP;  return 1; }
    if (msh_streq(s, "-STOP") || msh_streq(s, "-19"))  { *signo = SIGSTOP; return 1; }
    if (msh_streq(s, "-CONT") || msh_streq(s, "-18"))  { *signo = SIGCONT; return 1; }
    if (msh_streq(s, "-TSTP") || msh_streq(s, "-20"))  { *signo = SIGTSTP; return 1; }
    if (msh_streq(s, "-QUIT") || msh_streq(s, "-3"))   { *signo = SIGQUIT; return 1; }
    /* numeric: -N */
    if (s[1] >= '0' && s[1] <= '9') {
        *signo = atoi(s + 1);
        return 1;
    }
    msh_error("kill: %s: invalid signal", s);
    return 0;
}

static pid_t resolve_target(const char *spec, int *signo)
{
    if (spec[0] == '%') {
        msh_job_t *j = NULL;
        const char *id = spec + 1;
        if (!*id || msh_streq(id, "+")) j = msh_job_last_or_current();
        else if (msh_streq(id, "-"))    j = msh_job_previous();
        else if (msh_isdigit((unsigned char)id[0])) j = msh_job_find(atoi(id));
        else j = msh_job_find_by_prefix(id);
        if (!j) { msh_error("kill: %s: no such job", spec); return 0; }
        if (kill(-j->pgid, *signo) < 0) { msh_perror("kill"); return 0; }
        return j->pgid;
    }
    pid_t pid = (pid_t)atoi(spec);
    if (pid <= 0) { msh_error("kill: %s: invalid pid", spec); return 0; }
    if (kill(pid, *signo) < 0) { msh_perror("kill"); return 0; }
    return pid;
}

int msh_bi_kill(int argc, char **argv)
{
    int signo = SIGTERM;
    int i = 1;
    if (argc < 2) {
        msh_error("kill: usage: kill [-signal] pid | %%job");
        return 1;
    }
    if (argv[i][0] == '-' && (argv[i][1] < '0' || argv[i][1] > '9') && argv[i][1] != '\0') {
        if (!parse_signal(argv[i], &signo)) return 1;
        i++;
    } else if (argv[i][0] == '-' && argv[i][1] >= '0' && argv[i][1] <= '9') {
        if (!parse_signal(argv[i], &signo)) return 1;
        i++;
    }
    int rc = 0;
    for (; i < argc; i++) {
        if (!resolve_target(argv[i], &signo)) rc = 1;
    }
    return rc;
}