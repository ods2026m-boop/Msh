/*
 * Msh - builtin: alias / unalias
 */
#include "msh/builtin.h"
#include "msh/util.h"

int msh_bi_alias(int argc, char **argv)
{
    msh_state_t *st = msh_get_state();
    if (argc < 2) {
        for (int i = 0; i < st->alias_count; i++)
            printf("alias %s='%s'\n", st->aliases[i].name, st->aliases[i].value);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!eq) {
            for (int j = 0; j < st->alias_count; j++) {
                if (msh_streq(st->aliases[j].name, argv[i])) {
                    printf("alias %s='%s'\n", st->aliases[j].name, st->aliases[j].value);
                }
            }
            continue;
        }
        *eq = '\0';
        /* set or replace */
        int found = 0;
        for (int j = 0; j < st->alias_count; j++) {
            if (msh_streq(st->aliases[j].name, argv[i])) {
                free(st->aliases[j].value);
                st->aliases[j].value = msh_strdup(eq + 1);
                found = 1; break;
            }
        }
        if (!found) {
            if (st->alias_count < st->alias_capacity) {
                st->aliases[st->alias_count].name = msh_strdup(argv[i]);
                st->aliases[st->alias_count].value = msh_strdup(eq + 1);
                st->alias_count++;
            }
        }
        *eq = '=';
    }
    return 0;
}

int msh_bi_unalias(int argc, char **argv)
{
    if (argc < 2) { msh_error("unalias: usage: unalias NAME ..."); return 1; }
    msh_state_t *st = msh_get_state();
    for (int i = 1; i < argc; i++) {
        for (int j = 0; j < st->alias_count; j++) {
            if (msh_streq(st->aliases[j].name, argv[i])) {
                free(st->aliases[j].name);
                free(st->aliases[j].value);
                st->aliases[j] = st->aliases[--st->alias_count];
                break;
            }
        }
    }
    return 0;
}

/* Look up an alias by name. Returns value or NULL. */
static const char *alias_lookup(const char *name)
{
    msh_state_t *st = msh_get_state();
    for (int i = 0; i < st->alias_count; i++) {
        if (msh_streq(st->aliases[i].name, name)) return st->aliases[i].value;
    }
    return NULL;
}

/* Expand alias in argv[0] by tokenizing the alias value and replacing argv.
 * Supports a single level of expansion (no recursion) to avoid infinite loops. */
int msh_alias_expand(char ***argv, int *argc)
{
    if (!argv || !*argv || !(*argv)[0]) return 0;
    const char *val = alias_lookup((*argv)[0]);
    if (!val) return 0;
    /* tokenize val: simple split on whitespace */
    int ntok = 0;
    char **toks = msh_strsplit(val, " \t", &ntok);
    if (ntok == 0) { free(toks); return 0; }
    /* build new argv: tokens + original argv[1..argc] */
    int new_argc = ntok + (*argc - 1);
    char **new_argv = msh_xmalloc(sizeof(char*) * (new_argc + 1));
    for (int i = 0; i < ntok; i++) new_argv[i] = msh_strdup(toks[i]);
    for (int i = 1; i < *argc; i++) new_argv[ntok + i - 1] = msh_strdup((*argv)[i]);
    new_argv[new_argc] = NULL;
    /* free old argv */
    for (int i = 0; i < *argc; i++) free((*argv)[i]);
    free(*argv);
    *argv = new_argv;
    *argc = new_argc;
    msh_strfreev(toks);
    return 1;
}