/*
 * Msh - Pipeline execution.
 * Runs a chain of commands connected via pipes().
 */
#include "msh/pipeline.h"
#include "msh/util.h"
#include "msh/exec.h"
#include "msh/builtin.h"
#include "msh/redir.h"
#include "msh/expand.h"
#include "msh/job.h"

static int is_builtin_name(const char *name)
{
    if (!name) return 0;
    for (const msh_builtin_t *b = msh_builtins; b->name; b++)
        if (msh_streq(b->name, name)) return 1;
    return 0;
}

static int run_single_child(msh_cmd_t *cmd, int in_fd, int out_fd)
{
    if (in_fd != STDIN_FILENO)  { dup2(in_fd, STDIN_FILENO);  close(in_fd); }
    if (out_fd != STDOUT_FILENO) { dup2(out_fd, STDOUT_FILENO); close(out_fd); }
    msh_apply_redirs(cmd->redirs);
    char **argv = msh_xmalloc(sizeof(char*) * (cmd->argc + 1));
    for (int i = 0; i < cmd->argc; i++) argv[i] = msh_expand_vars(cmd->argv[i]);
    argv[cmd->argc] = NULL;
    int argc = cmd->argc;
    if (argc > 0) msh_alias_expand(&argv, &argc);
    if (is_builtin_name(argv[0])) {
        for (const msh_builtin_t *b = msh_builtins; b->name; b++) {
            if (msh_streq(b->name, argv[0])) {
                int r = b->fn(argc, argv);
                fflush(stdout); fflush(stderr);
                _exit(r);
            }
        }
        fflush(stdout); fflush(stderr);
        _exit(1);
    }
    char *path = msh_resolve_path(argv[0]);
    if (!path) { fprintf(stderr, "msh: %s: command not found\n", argv[0]); _exit(127); }
    execvp(path, argv);
    msh_perror(argv[0]);
    _exit(126);
}

char *msh_pipeline_to_string(msh_pipeline_t *pl)
{
    size_t cap = 256, n = 0;
    char *buf = msh_xmalloc(cap);
    for (int i = 0; i < pl->ncmds; i++) {
        msh_cmd_t *c = &pl->cmds[i];
        for (int j = 0; j < c->argc; j++) {
            size_t l = strlen(c->argv[j]);
            if (n + l + 3 >= cap) { cap *= 2; buf = msh_xrealloc(buf, cap); }
            if (j > 0) buf[n++] = ' ';
            memcpy(buf + n, c->argv[j], l);
            n += l;
        }
        if (i + 1 < pl->ncmds) {
            if (n + 4 >= cap) { cap *= 2; buf = msh_xrealloc(buf, cap); }
            memcpy(buf + n, " | ", 3); n += 3;
        }
    }
    buf[n] = '\0';
    return buf;
}

int msh_pipeline_run(msh_pipeline_t *pl, int background)
{
    if (!pl || pl->ncmds == 0) return 0;

    /* Single-command optimization: handled by exec.c. */
    if (pl->ncmds == 1) return -1;

    int (*pipes)[2] = msh_xmalloc(sizeof(int[2]) * (pl->ncmds - 1));
    for (int i = 0; i < pl->ncmds - 1; i++) {
        if (pipe(pipes[i]) < 0) { msh_perror("pipe"); free(pipes); return 1; }
    }

    pid_t *pids = msh_xmalloc(sizeof(pid_t) * pl->ncmds);
    pid_t pgid = 0;

    for (int i = 0; i < pl->ncmds; i++) {
        int in_fd  = (i == 0) ? STDIN_FILENO  : pipes[i-1][0];
        int out_fd = (i == pl->ncmds - 1) ? STDOUT_FILENO : pipes[i][1];

        pid_t pid = fork();
        if (pid < 0) { msh_perror("fork"); continue; }
        if (pid == 0) {
            run_single_child(&pl->cmds[i], in_fd, out_fd);
        }
        pids[i] = pid;
        if (pgid == 0) pgid = pid;
        setpgid(pid, pgid);
        /* parent closes its copies */
        if (in_fd  != STDIN_FILENO)  close(in_fd);
        if (out_fd != STDOUT_FILENO) close(out_fd);
    }
    free(pipes);

    char *cmdline = msh_pipeline_to_string(pl);
    int last_status = 0;
    if (!background) {
        for (int i = 0; i < pl->ncmds; i++) {
            int st;
            waitpid(pids[i], &st, 0);
            if (i == pl->ncmds - 1) {
                if (WIFEXITED(st)) last_status = WEXITSTATUS(st);
                else if (WIFSIGNALED(st)) last_status = 128 + WTERMSIG(st);
            }
        }
    } else {
        msh_job_add(pgid, cmdline);
    }
    free(pids);
    free(cmdline);
    return last_status;
}