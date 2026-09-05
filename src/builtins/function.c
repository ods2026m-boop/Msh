/*
 * Msh - builtin: function
 * Syntax: function NAME { BODY; }
 * Also recognized: NAME() { BODY; } (handled directly in parser/exec).
 */
#include "msh/builtin.h"
#include "msh/util.h"
#include "msh/function.h"
#include "msh/compound.h"
#include <stdio.h>

/* Read a function body from stdin until matching `}`. */
static char *read_func_body(void)
{
    char buf[MSH_MAX_LINE];
    size_t cap = 256, n = 0;
    char *body = msh_xmalloc(cap);
    int depth = 1;
    while (1) {
        if (!fgets(buf, sizeof(buf), stdin)) break;
        /* crude brace counting */
        char *p = buf;
        while (*p) {
            if (*p == '{') depth++;
            else if (*p == '}') { depth--; if (depth == 0) goto done; }
            p++;
        }
        size_t l = strlen(buf);
        if (n + l + 2 >= cap) { cap = (n + l + 2) * 2; body = msh_xrealloc(body, cap); }
        memcpy(body + n, buf, l); n += l;
        body[n] = '\0';
    }
done:
    return body;
}

int msh_bi_function(int argc, char **argv)
{
    if (argc < 3) { msh_error("function: usage: function NAME { BODY; }"); return 1; }
    const char *name = argv[1];
    /* expect { on next token - read body until } */
    if (msh_streq(argv[2], "{")) {
        /* argv[2] = "{", body is rest of argv + stdin */
        char *body = msh_xmalloc(256);
        size_t cap = 256, n = 0;
        for (int i = 3; i < argc; i++) {
            size_t l = strlen(argv[i]);
            if (n + l + 4 >= cap) { cap = (n + l + 4) * 2; body = msh_xrealloc(body, cap); }
            memcpy(body + n, argv[i], l); n += l;
            body[n++] = ' ';
        }
        body[n] = '\0';
        /* if last char is '}', trim */
        if (n > 0 && body[n-1] == '}') { body[n-1] = '\0'; n--; }
        msh_function_define(name, body);
        free(body);
        return 0;
    }
    /* read body from stdin */
    char *body = read_func_body();
    msh_function_define(name, body);
    free(body);
    return 0;
}