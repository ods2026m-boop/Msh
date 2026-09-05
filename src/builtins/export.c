/*
 * Msh - builtin: export
 */
#include "msh/builtin.h"
#include "msh/util.h"

int msh_bi_export(int argc, char **argv)
{
    if (argc < 2) {
        extern char **environ;
        for (char **e = environ; *e; e++) puts(*e);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!eq) {
            /* mark existing variable for export */
            setenv(argv[i], getenv(argv[i]) ? getenv(argv[i]) : "", 1);
            continue;
        }
        *eq = '\0';
        setenv(argv[i], eq + 1, 1);
        *eq = '=';
    }
    return 0;
}