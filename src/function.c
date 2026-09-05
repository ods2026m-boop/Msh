/*
 * Msh - Functions.
 *
 * Define functions with `name() { body; }` or `function name { body; }`.
 * Call them like regular commands: `name args`.
 * Stored in a simple linked list.
 */
#include "msh/util.h"
#include "msh/compound.h"
#include "msh/msh.h"
#include <stdlib.h>

typedef struct msh_func_s {
    char *name;
    char *body;
    int refcount;
    char *pending_free_body;
    struct msh_func_s *next;
} msh_func_t;

static msh_func_t *g_funcs = NULL;

static msh_func_t *func_find(const char *name)
{
    for (msh_func_t *f = g_funcs; f; f = f->next)
        if (msh_streq(f->name, name)) return f;
    return NULL;
}

/* Register or replace a function. body is duplicated. */
void msh_function_define(const char *name, const char *body)
{
    if (!name || !body) return;
    msh_func_t *f = func_find(name);
    if (f) {
        free(f->body);
        f->body = msh_strdup(body);
        return;
    }
    f = msh_xcalloc(1, sizeof(*f));
    f->name = msh_strdup(name);
    f->body = msh_strdup(body);
    f->refcount = 0;
    f->pending_free_body = NULL;
    f->next = g_funcs;
    g_funcs = f;
}

const char *msh_function_lookup(const char *name)
{
    msh_func_t *f = func_find(name);
    return f ? f->body : NULL;
}

int msh_function_call(const char *name, int argc, char **argv)
{
    msh_func_t *f = func_find(name);
    if (!f) return 127;
    /* positional parameters stored as MSHPARAM1..MSHPARAM9 (since env vars
     * can't start with a digit). expand.c translates $1..$9 to these. */
    msh_state_t *st = msh_get_state();
    for (int i = 0; i < 9; i++) {
        free(st->pos_params[i]);
        st->pos_params[i] = NULL;
    }
    int nargs = argc > 1 ? argc - 1 : 0;
    if (nargs > 9) nargs = 9;
    for (int i = 0; i < nargs; i++) {
        char varname[32];
        snprintf(varname, sizeof(varname), "MSHPARAM%d", i + 1);
        setenv(varname, argv[1 + i], 1);
        st->pos_params[i] = msh_strdup(argv[1 + i]);
    }
    return msh_compound_exec(f->body);
}

void msh_function_clear_all(void)
{
    while (g_funcs) {
        msh_func_t *f = g_funcs;
        g_funcs = f->next;
        if (f->refcount == 0) {
            free(f->name);
            free(f->body);
        } else {
            // Leak the name and body.
        }
        free(f);
    }
}