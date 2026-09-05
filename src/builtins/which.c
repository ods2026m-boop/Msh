/*
 * Msh - builtin: which
 */
#include "msh/builtin.h"
#include "msh/util.h"

int msh_bi_which(int argc, char **argv)
{
    if (argc < 2) return 1;
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        const char *path = getenv("PATH");
        if (path) {
            char *pdup = msh_strdup(path);
            char *save = NULL;
            int printed = 0;
            for (char *d = strtok_r(pdup, ":", &save); d; d = strtok_r(NULL, ":", &save)) {
                char *full = msh_path_join(d, argv[i]);
                if (msh_is_executable(full)) {
                    puts(full);
                    printed = 1;
                    free(full);
                    break;
                }
                free(full);
            }
            free(pdup);
            if (!printed) rc = 1;
        } else rc = 1;
    }
    return rc;
}