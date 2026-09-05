/*
 * Msh - builtin: cd
 */
#include "msh/builtin.h"
#include "msh/util.h"
#include <unistd.h>

int msh_bi_cd(int argc, char **argv)
{
    const char *target = NULL;
    if (argc < 2) {
        target = getenv("HOME");
        if (!target) {
            msh_error("cd: HOME not set");
            return 1;
        }
    } else if (msh_streq(argv[1], "-")) {
        target = getenv("OLDPWD");
        if (!target) {
            msh_error("cd: OLDPWD not set");
            return 1;
        }
        printf("%s\n", target);
    } else {
        target = argv[1];
    }
    char *old = msh_xmalloc(MSH_MAX_LINE);
    if (getcwd(old, MSH_MAX_LINE) == NULL) { free(old); return 1; }
    if (chdir(target) != 0) {
        msh_perror(target);
        free(old);
        return 1;
    }
    setenv("OLDPWD", old, 1);
    free(old);
    char *cwd = msh_xmalloc(MSH_MAX_LINE);
    if (getcwd(cwd, MSH_MAX_LINE)) setenv("PWD", cwd, 1);
    free(cwd);
    return 0;
}