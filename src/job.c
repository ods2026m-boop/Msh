/*
 * Msh - Job control
 */
#include "msh/job.h"
#include "msh/util.h"
#include "msh/signal.h"
#include <termios.h>

void msh_job_init(void) { /* nothing dynamic yet */ }

msh_job_t *msh_job_add(pid_t pgid, const char *cmdline)
{
    msh_state_t *st = msh_get_state();
    msh_job_t *j = msh_xcalloc(1, sizeof(*j));
    j->id = st->next_job_id++;
    j->pgid = pgid;
    j->cmdline = msh_strdup(cmdline ? cmdline : "");
    j->status = JOB_RUNNING;
    j->next = NULL;
    if (!st->jobs_head) {
        st->jobs_head = j;
        j->prev = NULL;
    } else {
        msh_job_t *t = st->jobs_head;
        while (t->next) t = t->next;
        t->next = j;
        j->prev = t;
    }
    return j;
}

void msh_job_remove(msh_job_t *j)
{
    if (!j) return;
    msh_state_t *st = msh_get_state();
    if (j->prev) j->prev->next = j->next;
    else st->jobs_head = j->next;
    if (j->next) j->next->prev = j->prev;
    free(j->cmdline);
    free(j);
}

msh_job_t *msh_job_find(int id)
{
    msh_state_t *st = msh_get_state();
    for (msh_job_t *j = st->jobs_head; j; j = j->next)
        if (j->id == id) return j;
    return NULL;
}

msh_job_t *msh_job_find_by_pgid(pid_t pgid)
{
    msh_state_t *st = msh_get_state();
    for (msh_job_t *j = st->jobs_head; j; j = j->next)
        if (j->pgid == pgid) return j;
    return NULL;
}

msh_job_t *msh_job_find_by_prefix(const char *prefix)
{
    if (!prefix) return NULL;
    msh_state_t *st = msh_get_state();
    /* match the most recent job whose cmdline starts with prefix */
    msh_job_t *best = NULL;
    for (msh_job_t *j = st->jobs_head; j; j = j->next) {
        if (msh_strstarts(j->cmdline, prefix)) {
            if (!best || j->id > best->id) best = j;
        }
    }
    return best;
}

msh_job_t *msh_job_last_or_current(void)
{
    msh_state_t *st = msh_get_state();
    if (!st->jobs_head) return NULL;
    msh_job_t *j = st->jobs_head;
    while (j->next) j = j->next;
    return j;
}

msh_job_t *msh_job_previous(void)
{
    msh_state_t *st = msh_get_state();
    if (!st->jobs_head) return NULL;
    msh_job_t *last = st->jobs_head;
    msh_job_t *prev = NULL;
    while (last->next) { prev = last; last = last->next; }
    return prev;
}

void msh_job_update_status(void)
{
    msh_state_t *st = msh_get_state();
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        /* getpgid() on a zombie fails; the bg job's pgid is set to pid
         * (see exec.c setpgid(pid, pid)). Try that first. */
        pid_t pgid = (pid_t)-1;
        pid_t g = getpgid(pid);
        if (g >= 0) pgid = g;
        else pgid = pid; /* fallback */
        msh_job_t *j = msh_job_find_by_pgid(pgid);
        if (!j) continue;
        if (WIFSTOPPED(status)) {
            j->status = JOB_STOPPED;
            if (WSTOPSIG(status) == SIGTSTP) fprintf(stderr, "\n[%d]  Stopped   %s\n", j->id, j->cmdline);
        } else if (WIFCONTINUED(status)) {
            j->status = JOB_RUNNING;
        } else {
            j->status = JOB_DONE;
            if (WIFEXITED(status)) fprintf(stderr, "[%d]  Done(%d)  %s\n", j->id, WEXITSTATUS(status), j->cmdline);
            else if (WIFSIGNALED(status)) fprintf(stderr, "[%d]  Killed(%d) %s\n", j->id, WTERMSIG(status), j->cmdline);
        }
    }
    /* remove DONE jobs */
    msh_job_t *j = st->jobs_head;
    while (j) {
        msh_job_t *n = j->next;
        if (j->status == JOB_DONE) msh_job_remove(j);
        j = n;
    }
}

void msh_job_print(msh_job_t *j)
{
    if (!j) return;
    const char *st = j->status == JOB_RUNNING ? "Running" :
                     j->status == JOB_STOPPED ? "Stopped" : "Done";
    printf("[%d]  %-7s  %s\n", j->id, st, j->cmdline);
}

void msh_job_print_all(void)
{
    msh_state_t *s = msh_get_state();
    for (msh_job_t *j = s->jobs_head; j; j = j->next) msh_job_print(j);
}

int msh_job_wait(msh_job_t *j)
{
    if (!j) return -1;
    int status = 0;
    int last_status = 0;
    pid_t pid;
    /* Wait for all processes in the group to exit. */
    for (;;) {
        pid = waitpid(-j->pgid, &status, WUNTRACED);
        if (pid < 0) {
            if (errno == EINTR) continue;
            if (errno == ECHILD) break;
            return -1;
        }
        if (WIFEXITED(status))   last_status = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) last_status = 128 + WTERMSIG(status);
        else if (WIFSTOPPED(status)) return 128 + WSTOPSIG(status);
    }
    return last_status;
}