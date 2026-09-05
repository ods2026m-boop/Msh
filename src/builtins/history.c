/*
 * Msh - builtin: history
 * Options:
 *   history           show all
 *   history N         show last N entries
 *   history -c        clear all
 *   history -d N      delete entry N
 *   history -w FILE   write to FILE
 *   history -r FILE   read from FILE
 */
#include "msh/builtin.h"
#include "msh/history.h"
#include "msh/util.h"
#include <fcntl.h>
#include <unistd.h>

int msh_bi_history(int argc, char **argv)
{
    msh_state_t *st = msh_get_state();
    int max = st->history_count;
    int i = 1;
    while (i < argc) {
        const char *a = argv[i];
        if (msh_streq(a, "-c")) { msh_history_clear(); return 0; }
        else if (msh_streq(a, "-d")) {
            if (i + 1 >= argc) { msh_error("history: -d requires an argument"); return 1; }
            int n = atoi(argv[++i]);
            msh_history_delete(n - 1); /* user-visible is 1-based */
            return 0;
        }
        else if (msh_streq(a, "-w")) {
            if (i + 1 >= argc) { msh_error("history: -w requires FILE"); return 1; }
            msh_history_save(argv[++i]);
            return 0;
        }
        else if (msh_streq(a, "-r")) {
            if (i + 1 >= argc) { msh_error("history: -r requires FILE"); return 1; }
            msh_history_load(argv[++i]);
            return 0;
        }
        else if (a[0] == '-' && a[1] >= '0' && a[1] <= '9') { i++; continue; }
        else if (msh_isdigit((unsigned char)a[0])) {
            int n = atoi(a);
            if (n > 0 && n < st->history_count) max = n;
            i++;
            continue;
        }
        else break;
    }

    int start = st->history_count - max;
    if (start < 0) start = 0;
    for (int k = start; k < st->history_count; k++)
        printf("%5d  %s\n", k + 1, st->history[k].line);
    fflush(stdout);
    return 0;
}