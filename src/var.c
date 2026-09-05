/*
 * Msh - Variables (env wrapper + simple local storage).
 */
#include "msh/var.h"
#include "msh/util.h"

char *msh_var_get(const char *name)
{
    if (!name) return NULL;
    const char *v = getenv(name);
    return v ? msh_strdup(v) : NULL;
}

int msh_var_set(const char *name, const char *value)
{
    if (!name) return -1;
    return setenv(name, value ? value : "", 1);
}

int msh_var_unset(const char *name)
{
    if (!name) return -1;
    return unsetenv(name);
}

char **msh_var_list(int *count)
{
    extern char **environ;
    int n = 0;
    for (char **e = environ; *e; e++) n++;
    char **out = msh_xmalloc(sizeof(char*) * (n + 1));
    int i = 0;
    for (char **e = environ; *e; e++) out[i++] = msh_strdup(*e);
    out[i] = NULL;
    if (count) *count = n;
    return out;
}

int msh_var_export(const char *name)
{
    if (!name) return -1;
    const char *v = getenv(name);
    if (!v) return -1;
    setenv(name, v, 1);
    return 0;
}