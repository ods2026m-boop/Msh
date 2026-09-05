/*
 * Msh - Signal handling.
 *
 * Strategy:
 *   - Use sigaction() (not signal()) for reliable handler semantics.
 *   - Handlers only set a flag or write one byte (async-signal-safe).
 *   - Actual work is done by msh_signal_dispatch() called from main loop.
 *   - SIGINT/SIGTSTP/SIGTTOU/SIGTTIN are forwarded to the foreground
 *     process group; the shell itself does not exit on Ctrl+C.
 */
#include "msh/signal.h"
#include "msh/util.h"
#include "msh/job.h"
#include "msh/exec.h"
#include <termios.h>

volatile sig_atomic_t msh_got_sigint  = 0;
volatile sig_atomic_t msh_got_sigtstp = 0;
volatile sig_atomic_t msh_got_sigchld = 0;
volatile sig_atomic_t msh_got_sighup  = 0;

/* PID of foreground process group (0 if no foreground job). */
static volatile pid_t fg_pgid = 0;

void msh_signal_set_fg(pid_t pgid) { fg_pgid = pgid; }

/* Async-signal-safe best-effort write.
 * write() is async-signal-safe, but in a signal handler we cannot
 * meaningfully retry partial writes or recover from EINTR/EPIPE, so
 * the return value is intentionally discarded. */
static void safe_write(const char *buf, size_t len)
{
    (void)write(STDOUT_FILENO, buf, len);
}

static void sigint_handler(int s)
{
    (void)s;
    msh_got_sigint = 1;
     if (fg_pgid > 0) kill(-fg_pgid, SIGINT);
     else {
         safe_write("\n", 1);
     }
}

static void sigtstp_handler(int s)
{
    (void)s;
    msh_got_sigtstp = 1;
     if (fg_pgid > 0) kill(-fg_pgid, SIGTSTP);
     else {
         safe_write("\n", 1);
     }
}

static void sigchld_handler(int s)
{
    (void)s;
    msh_got_sigchld = 1;
}

static void sigttou_handler(int s)
{
    (void)s;
    /* ignore — background jobs shouldn't write to terminal */
}

static void sigttin_handler(int s)
{
    (void)s;
    /* ignore */
}

static void sighup_handler(int s)
{
    (void)s;
    msh_got_sighup = 1;
}

static void sigquit_handler(int s)
{
    (void)s;
    /* Ctrl+\ — forward to foreground job (may dump core), don't kill shell */
    if (fg_pgid > 0) kill(-fg_pgid, SIGQUIT);
}

static int install(int sig, void (*fn)(int))
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = fn;
    sigemptyset(&sa.sa_mask);
    /* Block additional signals during handler execution */
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGCHLD);
    sa.sa_flags = SA_RESTART;
    return sigaction(sig, &sa, NULL);
}

void msh_signal_init(void)
{
    install(SIGINT,  sigint_handler);
    install(SIGTSTP, sigtstp_handler);
    install(SIGCHLD, sigchld_handler);
    install(SIGTTOU, sigttou_handler);
    install(SIGTTIN, sigttin_handler);
    install(SIGHUP,  sighup_handler);
    install(SIGQUIT, sigquit_handler);
    /* SIGTERM default — let kernel deliver it */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
}

/* Called from main loop after each command. Processes pending signals. */
void msh_signal_dispatch(void)
{
    if (msh_got_sigchld) {
        msh_got_sigchld = 0;
        msh_job_update_status();
    }
    if (msh_got_sigint) {
        msh_got_sigint = 0;
        if (isatty(STDIN_FILENO)) {
            extern char *msh_prompt_get(void);
            char *p = msh_prompt_get();
            safe_write("\n", 1);
            if (p) safe_write(p, strlen(p));
        }
    }
    if (msh_got_sigtstp) {
        msh_got_sigtstp = 0;
        if (isatty(STDIN_FILENO)) {
            safe_write("\n", 1);
        }
    }
    if (msh_got_sighup) {
        msh_got_sighup = 0;
        /* Tell all jobs to terminate */
        msh_state_t *st = msh_get_state();
        for (msh_job_t *j = st->jobs_head; j; j = j->next) {
            kill(-j->pgid, SIGHUP);
        }
    }
}

void msh_signal_block(void)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTSTP);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGTTOU);
    sigaddset(&set, SIGTTIN);
    sigprocmask(SIG_BLOCK, &set, NULL);
}

void msh_signal_unblock(void)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTSTP);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGTTOU);
    sigaddset(&set, SIGTTIN);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}