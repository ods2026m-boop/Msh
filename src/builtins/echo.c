/*
 * Msh - builtin: echo
 */
#include "msh/builtin.h"
#include "msh/util.h"

int msh_bi_echo(int argc, char **argv)
{
    int newline = 1;
    int escape  = 0;
    int start = 1;
    if (argc > 1 && argv[1][0] == '-') {
        for (const char *p = argv[1] + 1; *p; p++) {
            if (*p == 'n') newline = 0;
            else if (*p == 'e') escape = 1;
            else if (*p == 'E') escape = 0;
        }
        start = 2;
    }
    for (int i = start; i < argc; i++) {
        if (i > start) putchar(' ');
        if (escape) {
            for (const char *p = argv[i]; *p; p++) {
                if (*p == '\\' && p[1]) {
                    switch (p[1]) {
                        case 'n': putchar('\n'); p++; break;
                        case 't': putchar('\t'); p++; break;
                        case 'r': putchar('\r'); p++; break;
                        case '\\': putchar('\\'); p++; break;
                        case '0': putchar('\0'); p++; break;
                        default:  putchar(*p); break;
                    }
                } else putchar(*p);
            }
        } else {
            fputs(argv[i], stdout);
        }
    }
    if (newline) putchar('\n');
    return 0;
}