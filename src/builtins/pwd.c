/*
 * Msh - builtin: pwd
 */
#include "msh/builtin.h"
#include "msh/util.h"

int msh_bi_pwd(int argc, char **argv)
{
    (void)argc; (void)argv;
    char buf[MSH_MAX_LINE];
    if (getcwd(buf, sizeof(buf))) {
        puts(buf);
        return 0;
    }
    msh_perror("pwd");
    return 1;
}