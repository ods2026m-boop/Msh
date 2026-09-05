/*
 * Msh - builtin: shopt
 * Toggle shell options. Supports a small subset:
 *   - nullglob:    if set, globs that match nothing expand to nothing
 *   - extglob:     if set, extended glob patterns (?, *, +, @, !) work
 *   - dotglob:     if set, globs match hidden files
 *   - nocaseglob:  case-insensitive glob
 *   - errexit:     exit on error
 *   - xtrace:      print commands before execution
 *   - verbose:     print input lines as read
 */
#include "msh/builtin.h"
#include "msh/util.h"

typedef struct {
    const char *name;
    int        *flag;
    const char *desc;
} msh_shoptt_t;

extern int msh_opt_nullglob;
extern int msh_opt_extglob;
extern int msh_opt_dotglob;
extern int msh_opt_nocaseglob;
extern int msh_opt_errexit;
extern int msh_opt_xtrace;

static const msh_shoptt_t options[] = {
    { "nullglob",   &msh_opt_nullglob,   "empty glob expands to nothing" },
    { "extglob",    &msh_opt_extglob,    "extended glob operators" },
    { "dotglob",    &msh_opt_dotglob,    "glob matches dot files" },
    { "nocaseglob", &msh_opt_nocaseglob, "case-insensitive glob" },
    { "errexit",    &msh_opt_errexit,    "exit on error" },
    { "xtrace",     &msh_opt_xtrace,     "print commands before execution" },
    { NULL, NULL, NULL }
};

int msh_bi_shopt(int argc, char **argv)
{
    /* shopt with no args: list all */
    if (argc < 2) {
        for (int i = 0; options[i].name; i++) {
            printf("%s  %s\n", *options[i].flag ? "on " : "off",
                   options[i].name);
        }
        return 0;
    }
    int enable = -1;
    int i = 1;
    if (msh_streq(argv[i], "-s")) { enable = 1; i++; }
    else if (msh_streq(argv[i], "-u")) { enable = 0; i++; }
    else if (msh_streq(argv[i], "-q")) { enable = -2; i++; } /* query */
    for (; i < argc; i++) {
        int found = 0;
        for (int j = 0; options[j].name; j++) {
            if (msh_streq(options[j].name, argv[i])) {
                if (enable == -2) return *options[j].flag ? 0 : 1;
                if (enable >= 0) *options[j].flag = enable;
                else printf("%s  %s\n", *options[j].flag ? "on " : "off", options[j].name);
                found = 1;
                break;
            }
        }
        if (!found) {
            msh_error("shopt: %s: invalid shell option", argv[i]);
            return 1;
        }
    }
    return 0;
}