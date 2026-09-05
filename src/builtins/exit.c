/*
 * Msh - builtin: exit
 */
#include "msh/builtin.h"
#include "msh/util.h"

int msh_bi_exit(int argc, char **argv)
{
    msh_state_t *st = msh_get_state();
    int code = 0;
    if (argc > 1) {
        code = atoi(argv[1]);
        if (argc > 2) {
            msh_error("exit: too many arguments");
            return 1;
        }
    } else {
        code = st->last_status;
    }
    st->exit_code = code;
    st->should_exit = 1;
    return code;
}