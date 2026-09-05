/*
 * Msh - builtin: unset
 */
#include "msh/builtin.h"
#include "msh/util.h"

int msh_bi_unset(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) unsetenv(argv[i]);
    return 0;
}