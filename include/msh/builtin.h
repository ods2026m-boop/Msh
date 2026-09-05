#ifndef MSH_BUILTIN_H
#define MSH_BUILTIN_H

#include "msh.h"

typedef int (*msh_builtin_fn)(int argc, char **argv);

typedef struct {
    const char    *name;
    msh_builtin_fn fn;
} msh_builtin_t;

/* Built-in table */
extern const msh_builtin_t msh_builtins[];

/* individual builtins */
int msh_bi_cd(int argc, char **argv);
int msh_bi_echo(int argc, char **argv);
int msh_bi_exit(int argc, char **argv);
int msh_bi_pwd(int argc, char **argv);
int msh_bi_export(int argc, char **argv);
int msh_bi_unset(int argc, char **argv);
int msh_bi_alias(int argc, char **argv);
int msh_bi_unalias(int argc, char **argv);
int msh_bi_history(int argc, char **argv);
int msh_bi_jobs(int argc, char **argv);
int msh_bi_fg(int argc, char **argv);
int msh_bi_bg(int argc, char **argv);
int msh_bi_type(int argc, char **argv);
int msh_bi_which(int argc, char **argv);
int msh_bi_source(int argc, char **argv);
int msh_bi_kill(int argc, char **argv);
int msh_bi_umask(int argc, char **argv);
int msh_bi_shopt(int argc, char **argv);
int msh_bi_test(int argc, char **argv);
int msh_bi_function(int argc, char **argv);

/* Shell option flags (read/written by shopt). */
extern int msh_opt_nullglob;
extern int msh_opt_extglob;
extern int msh_opt_dotglob;
extern int msh_opt_nocaseglob;
extern int msh_opt_errexit;
extern int msh_opt_xtrace;

/* Resolve aliases: if argv[0] is an alias, expand it in place.
 * Returns 1 if an expansion happened (argv is reallocated), 0 otherwise. */
int msh_alias_expand(char ***argv, int *argc);

#endif