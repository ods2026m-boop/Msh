/*
 * Msh - Arrays.
 *
 * Supports:
 *   arr=(a b c)         assignment
 *   arr[2]=value        indexed assignment
 *   ${arr[0]}           element access
 *   ${arr[@]}           all elements
 *   ${#arr[@]}          array length
 *
 * Storage: per-name dynamic array in shell state.
 */
#include "msh/util.h"
#include "msh/builtin.h"
#include <stdlib.h>

typedef struct msh_array_s {
    char *name;
    char **items;
    int   count;
    int   cap;
    struct msh_array_s *next;
} msh_array_t;

/* global linked list of arrays (kept simple) */
static msh_array_t *g_arrays = NULL;

static msh_array_t *array_find(const char *name)
{
    for (msh_array_t *a = g_arrays; a; a = a->next)
        if (msh_streq(a->name, name)) return a;
    return NULL;
}

static msh_array_t *array_new(const char *name)
{
    msh_array_t *a = msh_xcalloc(1, sizeof(*a));
    a->name = msh_strdup(name);
    a->cap = 8;
    a->items = msh_xcalloc(a->cap, sizeof(char*));
    a->next = g_arrays;
    g_arrays = a;
    return a;
}

/* Set element at index, growing if needed. */
void msh_array_set(const char *name, int index, const char *value)
{
    if (index < 0) return;
    msh_array_t *a = array_find(name);
    if (!a) a = array_new(name);
    while (index >= a->cap) {
        int newcap = a->cap * 2;
        a->items = msh_xrealloc(a->items, sizeof(char*) * newcap);
        for (int i = a->cap; i < newcap; i++) a->items[i] = NULL;
        a->cap = newcap;
    }
    free(a->items[index]);
    a->items[index] = msh_strdup(value);
    if (index + 1 > a->count) a->count = index + 1;
}

/* Get element. Returns NULL if out of range. */
const char *msh_array_get(const char *name, int index)
{
    msh_array_t *a = array_find(name);
    if (!a) return NULL;
    if (index < 0 || index >= a->count) return NULL;
    return a->items[index];
}

/* Get all elements joined by IFS (default space). Caller frees. */
char *msh_array_join(const char *name)
{
    msh_array_t *a = array_find(name);
    if (!a) return msh_strdup("");
    size_t total = 0;
    for (int i = 0; i < a->count; i++) if (a->items[i]) total += strlen(a->items[i]) + 1;
    char *out = msh_xmalloc(total + 1);
    size_t n = 0;
    for (int i = 0; i < a->count; i++) {
        if (!a->items[i]) continue;
        if (n > 0) out[n++] = ' ';
        size_t l = strlen(a->items[i]);
        memcpy(out + n, a->items[i], l);
        n += l;
    }
    out[n] = '\0';
    return out;
}

int msh_array_count(const char *name)
{
    msh_array_t *a = array_find(name);
    return a ? a->count : 0;
}

void msh_array_clear(const char *name)
{
    msh_array_t *a = array_find(name);
    if (!a) return;
    for (int i = 0; i < a->count; i++) { free(a->items[i]); a->items[i] = NULL; }
    a->count = 0;
}

/* Parse "arr=(...)" or "arr[index]=value" forms.
 * Called from exec.c when a leading assignment-like token is detected. */
void msh_array_handle_assignment(const char *assignment)
{
    /* form: name=(... )  or  name[idx]=value  */
    const char *eq = strchr(assignment, '=');
    if (!eq) return;
    char *name = msh_strndup(assignment, eq - assignment);
    const char *rhs = eq + 1;
    /* check for [idx] in name */
    char *lb = strchr(name, '[');
    if (lb) {
        *lb = '\0';
        char *rb = strchr(lb + 1, ']');
        if (!rb) { free(name); return; }
        *rb = '\0';
        int idx = atoi(lb + 1);
        msh_array_set(name, idx, rhs);
    } else if (*rhs == '(') {
        /* list form: ( a b c ) */
        const char *end = strrchr(rhs, ')');
        if (!end) { free(name); return; }
        msh_array_clear(name);
        msh_array_t *a = array_find(name);
        if (!a) a = array_new(name);
        const char *p = rhs + 1;
        while (p < end) {
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\n')) p++;
            const char *q = p;
            while (q < end && *q != ' ' && *q != '\t' && *q != '\n') q++;
            if (q > p) {
                char *tok = msh_strndup(p, q - p);
                /* expand vars */
                extern char *msh_expand_vars(const char *s);
                char *expanded = msh_expand_vars(tok);
                msh_array_set(name, a->count, expanded);
                free(expanded); free(tok);
            }
            p = q;
        }
    } else {
        /* scalar assignment with same name as array; store as arr[0] */
        msh_array_set(name, 0, rhs);
    }
    free(name);
}

void msh_array_cleanup_all(void)
{
    while (g_arrays) {
        msh_array_t *a = g_arrays;
        g_arrays = a->next;
        for (int i = 0; i < a->count; i++) free(a->items[i]);
        free(a->items);
        free(a->name);
        free(a);
    }
}