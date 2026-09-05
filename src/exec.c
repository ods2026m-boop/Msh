/*
 * Msh - Executor: runs AST pipelines using fork/exec/pipe.
 */
#include "msh/exec.h"
#include "msh/util.h"
#include "msh/builtin.h"
#include "msh/redir.h"
#include "msh/job.h"
#include "msh/signal.h"
#include "msh/expand.h"
#include "msh/var.h"
#include "msh/pipeline.h"
#include "msh/array.h"
#include "msh/function.h"

static int is_builtin_name(const char *name)
{
    if (!name) return 0;
    for (const msh_builtin_t *b = msh_builtins; b->name; b++)
        if (msh_streq(b->name, name)) return 1;
    return 0;
}

static int run_builtin_in_child(int argc, char **argv, msh_redir_t *r)
{
    if (msh_apply_redirs(r) < 0) return 1;
    for (const msh_builtin_t *b = msh_builtins; b->name; b++) {
        if (msh_streq(b->name, argv[0])) return b->fn(argc, argv);
    }
    return 1;
}

int msh_apply_redirs(msh_redir_t *r)
{
    for (; r; r = r->next) if (msh_open_redir(r) < 0) return -1;
    return 0;
}

char *msh_resolve_path(const char *cmd)
{
    if (!cmd) return NULL;
    if (strchr(cmd, '/')) {
        if (msh_is_executable(cmd)) return msh_strdup(cmd);
        return NULL;
    }
    const char *path = getenv("PATH");
    if (!path) return NULL;
    char *pdup = msh_strdup(path);
    char *save = NULL;
    for (char *d = strtok_r(pdup, ":", &save); d; d = strtok_r(NULL, ":", &save)) {
        char *full = msh_path_join(d, cmd);
        if (msh_is_executable(full)) { free(pdup); return full; }
        free(full);
    }
    free(pdup);
    return NULL;
}

int msh_is_builtin(const char *cmd) { return is_builtin_name(cmd); }

int msh_run_builtin(int argc, char **argv)
{
    for (const msh_builtin_t *b = msh_builtins; b->name; b++)
        if (msh_streq(b->name, argv[0])) return b->fn(argc, argv);
    return 1;
}

static int run_single_command(msh_cmd_t *cmd, int in_fd, int out_fd, int background, msh_pipeline_t *pl)
{
    /* expand vars in argv (own copies for child; original freed by parser) */
    char **argv = msh_xmalloc(sizeof(char*) * (cmd->argc + 1));
    for (int i = 0; i < cmd->argc; i++) argv[i] = msh_strdup(cmd->argv[i]);
    argv[cmd->argc] = NULL;
    int argc = cmd->argc;

    /* Reconstruct $((...)) sequences that the lexer split apart.
     * If argv[0] ends with '=' (e.g. "i=$"), join following argv[i] until
     * the $(( ... )) expression is complete. */
    if (argc > 0 && argv[0]) {
        char *eq = strrchr(argv[0], '=');
        if (eq && *(eq + 1) == '\0' && argc > 1) {
            int no_space = 1; /* argv[0] ends with $, so no space */
            size_t total = strlen(argv[0]) + strlen(argv[1]) + 1;
            char *joined = msh_xmalloc(total);
            snprintf(joined, total, "%s%s", argv[0], argv[1]);
            free(argv[0]);
            argv[0] = joined;
            for (int k = 1; k < argc; k++) argv[k] = argv[k+1];
            argc--;
            while (argc > 1) {
                const char *val = strchr(argv[0], '=') + 1;
                if (val[0] == '$' && strstr(val, "((") && strstr(val, "))")) break;
                if (val[0] == '(' && strchr(val, ')')) break;
                if (val[0] != '$' && val[0] != '(') break;
                size_t total2 = strlen(argv[0]) + strlen(argv[1]) + (no_space ? 1 : 2);
                char *joined2 = msh_xmalloc(total2);
                snprintf(joined2, total2, "%s%s%s", argv[0], no_space ? "" : " ", argv[1]);
                free(argv[0]);
                argv[0] = joined2;
                for (int k = 1; k < argc; k++) argv[k] = argv[k+1];
                argc--;
            }
        }
    }

    /* alias expansion (single level) */
    if (argc > 0) msh_alias_expand(&argv, &argc);

    /* expand vars/arith in each argv element */
    for (int i = 0; i < argc; i++) {
        char *e = msh_expand_vars(argv[i]);
        free(argv[i]);
        argv[i] = e;
    }

    /* function call: if a user-defined function exists, run it. */
    if (argc > 0 && msh_function_lookup(argv[0])) {
        int rc = msh_function_call(argv[0], argc, argv);
        for (int i = 0; i < argc; i++) free(argv[i]);
        free(argv);
        return rc;
    }

    /* builtin handling (last in pipeline only) */
    if (argc > 0 && is_builtin_name(argv[0]) && in_fd == STDIN_FILENO && out_fd == STDOUT_FILENO && !background) {
        int rc = run_builtin_in_child(argc, argv, cmd->redirs);
        for (int i = 0; i < argc; i++) free(argv[i]);
        free(argv);
        return rc;
    }

    pid_t pid = fork();
    if (pid < 0) { msh_perror("fork"); for (int i=0;i<argc;i++) free(argv[i]); free(argv); return 1; }
    if (pid == 0) {
        /* child */
        if (in_fd != STDIN_FILENO) { dup2(in_fd, STDIN_FILENO); close(in_fd); }
        if (out_fd != STDOUT_FILENO) { dup2(out_fd, STDOUT_FILENO); close(out_fd); }
        /* Put child in its own process group so it can become the foreground
         * group of the terminal. */
        setpgid(0, 0);
        msh_apply_redirs(cmd->redirs);
        if (is_builtin_name(argv[0])) {
            int r = run_builtin_in_child(argc, argv, NULL);
            fflush(stdout); fflush(stderr);
            _exit(r);
        }
        char *path = msh_resolve_path(argv[0]);
        if (!path) {
            fprintf(stderr, "msh: %s: command not found\n", argv[0]);
            _exit(127);
        }
        execvp(path, argv);
        msh_perror(argv[0]);
        _exit(126);
    }
    /* parent: ensure pgid is set even if child hasn't run yet */
    setpgid(pid, pid);
    if (background) {
        char *cl = msh_pipeline_to_string(pl);
        msh_job_add(pid, cl);
        free(cl);
        return 0;
    }
    /* become foreground pg */
    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, pid);
    msh_signal_set_fg(pid);
    int status = 0;
    waitpid(pid, &status, WUNTRACED);
    msh_signal_set_fg(0);
    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, getpgrp());
    for (int i = 0; i < argc; i++) free(argv[i]);
    free(argv);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    if (WIFSTOPPED(status)) {
        msh_state_t *st = msh_get_state();
        char *cl = msh_pipeline_to_string(pl);
        msh_job_add(pid, cl);
        free(cl);
        (void)st;
        fprintf(stderr, "\n[%d]  Stopped   %s\n", msh_job_find_by_pgid(pid) ? msh_job_find_by_pgid(pid)->id : 0, cl ? "" : "");
    }
    return 1;
}

int msh_exec_pipeline(msh_pipeline_t *pl, int background)
{
    if (pl->ncmds == 0) return 0;
    if (pl->ncmds > 1) return msh_pipeline_run(pl, background);

    /* single command path */
    char *cmdline = msh_pipeline_to_string(pl);
    int rc = run_single_command(&pl->cmds[0], STDIN_FILENO, STDOUT_FILENO, background, pl);
    free(cmdline);
    return rc;
}

int msh_exec_ast(msh_ast_t *ast)
{
    if (!ast || !ast->head) return 0;
    int last_status = 0;
    for (msh_pipeline_t *pl = ast->head; pl; pl = pl->next) {
/* Detect leading variable assignment: VAR=value cmd args
 * On the first command of EVERY pipeline. */
        if (pl->ncmds > 0) {
            msh_cmd_t *c = &pl->cmds[0];
            int n_assign = 0;
/* First, join tokens split by the lexer at $(( ... )) boundaries
             * and at (...) array list boundaries. */
            for (int i = 0; i + 1 < c->argc; i++) {
                char *eq = strchr(c->argv[i], '=');
                if (!eq) continue;
                const char *val = eq + 1;
                int need_join = 0;
                int no_space = 0;
                /* assignment value starts with $ (e.g. i=$) — needs further tokens */
                if (*val == '$') { need_join = 1; no_space = 1; }
                /* assignment value starts with ( (array list) — needs further tokens with space */
                else if (*val == '(' && !strchr(val, ')')) need_join = 1;
                if (!need_join) continue;
                /* join remaining tokens. For $((...)), no space (i=$((expr))). */
                int joined_any = 0;
                while (i + 1 < c->argc) {
                    const char *sep = no_space ? "" : " ";
                    size_t total = strlen(c->argv[i]) + strlen(c->argv[i+1]) + strlen(sep) + 1;
                    char *joined = msh_xmalloc(total);
                    snprintf(joined, total, "%s%s%s", c->argv[i], sep, c->argv[i+1]);
                    free(c->argv[i]);
                    c->argv[i] = joined;
                    for (int k = i + 1; k < c->argc; k++) c->argv[k] = c->argv[k+1];
                    c->argc--;
                    joined_any = 1;
                    /* check if now complete */
                    val = strchr(c->argv[i], '=') + 1;
                    if (val[0] == '$' && strstr(val, "((") && strstr(val, "))")) break;
                    if (val[0] == '(' && strchr(val, ')')) break;
                    if (val[0] != '$' && val[0] != '(') break;
                }
                if (!joined_any) continue; /* avoid infinite loop */
            }
            while (n_assign < c->argc) {
                char *eq = strchr(c->argv[n_assign], '=');
                if (!eq || eq == c->argv[n_assign]) break;
                *eq = '\0';
                const char *name = c->argv[n_assign];
                /* validate name: allow letters, digits, _, [, ] */
                int valid = (*name == '_' || (*name >= 'a' && *name <= 'z') || (*name >= 'A' && *name <= 'Z'));
                for (const char *p = name + 1; valid && *p; p++) {
                    if (!(msh_isalnum((unsigned char)*p) || *p == '_' || *p == '[' || *p == ']')) valid = 0;
                }
                if (!valid) { *eq = '='; break; }
                /* array assignment if name has [ ... ] or value starts with ( */
                int is_arr = (strchr(name, '[') && strchr(c->argv[n_assign], ']')) ||
                             *(eq + 1) == '(';
                if (is_arr) {
                    *eq = '=';
                    msh_array_handle_assignment(c->argv[n_assign]);
                    n_assign++;
                    continue;
                }
                char *expanded_value = msh_expand_vars(eq + 1);
                setenv(name, expanded_value ? expanded_value : "", 1);
                free(expanded_value);
                *eq = '=';
                n_assign++;
            }
            if (n_assign > 0) {
                /* shift remaining args down */
                for (int i = n_assign; i <= c->argc; i++)
                    c->argv[i - n_assign] = c->argv[i];
                c->argc -= n_assign;
                if (c->argc == 0 && !c->redirs) continue; /* pure assignment */
            }
        }
        last_status = msh_exec_pipeline(pl, pl->background);
        msh_get_state()->last_status = last_status;
    }
    return last_status;
}