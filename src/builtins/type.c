/*
 * Msh - builtin: type
 * Indicates how a command would be interpreted: builtin, alias, or external.
 */
#include "msh/builtin.h"
#include "msh/util.h"
#include <sys/stat.h>

int msh_bi_type(int argc, char **argv)
{
    if (argc < 2) return 1;
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        const char *name = argv[i];
        int found = 0;
        /* aliases */
        msh_state_t *st = msh_get_state();
        for (int j = 0; j < st->alias_count; j++) {
            if (msh_streq(st->aliases[j].name, name)) {
                printf("%s is an alias for '%s'\n", name, st->aliases[j].value);
                found = 1;
                break;
            }
        }
        if (found) continue;
        /* builtins */
        for (const msh_builtin_t *b = msh_builtins; b->name; b++) {
            if (msh_streq(b->name, name)) {
                printf("%s is a shell builtin\n", name);
                found = 1;
                break;
            }
        }
        if (found) continue;
        /* keywords (none for now) */
        /* PATH executables */
        const char *path = getenv("PATH");
        if (path) {
            char *pdup = msh_strdup(path);
            char *save = NULL;
            for (char *d = strtok_r(pdup, ":", &save); d; d = strtok_r(NULL, ":", &save)) {
                char *full = msh_path_join(d, name);
                if (msh_is_executable(full)) {
                    printf("%s is %s\n", name, full);
                    found = 1;
                    free(full);
                    break;
                }
                free(full);
            }
            free(pdup);
        }
        if (!found) {
            printf("type: %s: not found\n", name);
            rc = 1;
        }
    }
    return rc;
}