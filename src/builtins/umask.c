/*
 * Msh - builtin: umask
 * Display or set the file creation mode mask.
 */
#include "msh/builtin.h"
#include "msh/util.h"
#include <sys/types.h>
#include <sys/stat.h>

static int octal_value(const char *s)
{
    int v = 0;
    while (*s) {
        if (*s < '0' || *s > '7') return -1;
        v = v * 8 + (*s - '0');
        s++;
    }
    return v;
}

int msh_bi_umask(int argc, char **argv)
{
    if (argc < 2) {
        mode_t m = umask(0);
        umask(m);
        printf("%04o\n", (unsigned)(m & 07777));
        return 0;
    }
    int v;
    /* try octal first, then symbolic */
    v = octal_value(argv[1]);
    if (v < 0) {
        /* parse simple symbolic: u=rwx,g=rx,o=r */
        mode_t m = umask(0); umask(m);
        mode_t newm = m;
        const char *p = argv[1];
        while (*p) {
            mode_t who = 0, perms = 0;
            switch (*p) {
                case 'u': who = 0700; break;
                case 'g': who = 0070; break;
                case 'o': who = 0007; break;
                case 'a': who = 0777; break;
                default: msh_error("umask: bad who %c", *p); return 1;
            }
            p++;
            if (*p != '=') { msh_error("umask: expected ="); return 1; }
            p++;
            while (*p && *p != ',') {
                switch (*p) {
                    case 'r': perms |= (who & 0444) >> (who > 0700 ? 6 : (who > 0070 ? 3 : 0)); break;
                    case 'w': perms |= (who & 0222) >> (who > 0700 ? 6 : (who > 0070 ? 3 : 0)); break;
                    case 'x': perms |= (who & 0111) >> (who > 0700 ? 6 : (who > 0070 ? 3 : 0)); break;
                    default: msh_error("umask: bad perm %c", *p); return 1;
                }
                p++;
            }
            newm = (newm & ~who) | perms;
            if (*p == ',') p++;
        }
        v = newm;
    }
    umask((mode_t)(v & 07777));
    return 0;
}