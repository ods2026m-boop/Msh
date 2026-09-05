/*
 * Msh - builtin: test
 * Evaluate conditional expressions.
 * Supports: -f, -d, -e, -r, -w, -x, -s, -z, -n, =, !=, -eq, -ne, -gt, -lt
 * Connectives: -a (and), -o (or), ! (not).
 * Also available as `[` for compatibility.
 */
#include "msh/builtin.h"
#include "msh/util.h"
#include <sys/stat.h>
#include <unistd.h>

static int test_file(const char *path, int op)
{
    struct stat st;
    int exists = (stat(path, &st) == 0);
    switch (op) {
        case 'e': return exists;
        case 'f': return exists && S_ISREG(st.st_mode);
        case 'd': return exists && S_ISDIR(st.st_mode);
        case 'r': return exists && access(path, R_OK) == 0;
        case 'w': return exists && access(path, W_OK) == 0;
        case 'x': return exists && access(path, X_OK) == 0;
        case 's': return exists && st.st_size > 0;
        case 'L': return exists && S_ISLNK(st.st_mode);
        default:  return 0;
    }
}

static int eval_unary(char **argv, int *i)
{
    /* argv[*i] is the operator; argv[*i+1] is the operand */
    const char *op = argv[*i];
    const char *arg = argv[*i + 1];
    if (!arg) return 1;
    if (msh_streq(op, "-z")) return arg[0] != '\0';
    if (msh_streq(op, "-n")) return arg[0] == '\0';
    return !test_file(arg, op[1]);
}

static int eval_binary(char **argv, int *i)
{
    /* argv[*i] left, argv[*i+1] op, argv[*i+2] right */
    const char *left = argv[*i];
    const char *op   = argv[*i + 1];
    const char *right = argv[*i + 2];
    if (!right) return 0;
    if (msh_streq(op, "=")  || msh_streq(op, "==")) return !msh_streq(left, right);
    if (msh_streq(op, "!=")) return msh_streq(left, right);
    long lv = atol(left), rv = atol(right);
    if (msh_streq(op, "-eq")) return lv != rv;
    if (msh_streq(op, "-ne")) return lv == rv;
    if (msh_streq(op, "-gt")) return lv <= rv;
    if (msh_streq(op, "-lt")) return lv >= rv;
    if (msh_streq(op, "-ge")) return lv <  rv;
    if (msh_streq(op, "-le")) return lv >  rv;
    return 1;
}

static const char *unary_ops[] = {
    "-e","-f","-d","-r","-w","-x","-s","-z","-n","-L", NULL
};

static int is_unary(const char *s) {
    for (int i = 0; unary_ops[i]; i++) if (msh_streq(unary_ops[i], s)) return 1;
    return 0;
}
static int is_binary(const char *s) {
    return msh_streq(s, "=") || msh_streq(s, "==") || msh_streq(s, "!=") ||
           msh_streq(s, "-eq") || msh_streq(s, "-ne") ||
           msh_streq(s, "-gt") || msh_streq(s, "-lt") ||
           msh_streq(s, "-ge") || msh_streq(s, "-le");
}

int msh_bi_test(int argc, char **argv)
{
    /* strip trailing ']' if first arg is '[' */
    int start = 1;
    if (argc > 1 && msh_streq(argv[1], "[")) start = 2;
    int end = argc;
    if (end > start && msh_streq(argv[end-1], "]")) end--;

    /* simple recursive-descent evaluator */
    /* Find the lowest precedence operator (-o) and split */
    int depth = 0;
    int split = -1;
    for (int i = start; i < end; i++) {
        if (msh_streq(argv[i], "(")) depth++;
        else if (msh_streq(argv[i], ")")) depth--;
        else if (depth == 0 && msh_streq(argv[i], "-o")) { split = i; break; }
    }
    if (split >= 0) {
        /* recursively evaluate each side */
        char *largv[64], *rargv[64];
        int llen = split - start;
        int rlen = end - split - 1;
        if (llen > 60 || rlen > 60) return 1;
        largv[0] = (char*)"test"; for (int i = 0; i < llen; i++) largv[i+1] = argv[start+i]; largv[llen+1] = NULL;
        rargv[0] = (char*)"test"; for (int i = 0; i < rlen; i++) rargv[i+1] = argv[split+1+i]; rargv[rlen+1] = NULL;
        return msh_bi_test(llen + 1, largv) || msh_bi_test(rlen + 1, rargv);
    }

    /* look for -a (and) */
    for (int i = start; i < end; i++) {
        if (msh_streq(argv[i], "-a")) {
            char *largv[64], *rargv[64];
            int llen = i - start, rlen = end - i - 1;
            if (llen > 60 || rlen > 60) return 1;
            largv[0] = (char*)"test"; for (int k = 0; k < llen; k++) largv[k+1] = argv[start+k]; largv[llen+1] = NULL;
            rargv[0] = (char*)"test"; for (int k = 0; k < rlen; k++) rargv[k+1] = argv[i+1+k]; rargv[rlen+1] = NULL;
            return msh_bi_test(llen + 1, largv) && msh_bi_test(rlen + 1, rargv);
        }
    }

    /* negation */
    if (end - start >= 1 && msh_streq(argv[start], "!")) {
        char *nargv[64];
        int nlen = end - start - 1;
        if (nlen > 60) return 1;
        nargv[0] = (char*)"test"; for (int i = 0; i < nlen; i++) nargv[i+1] = argv[start+1+i]; nargv[nlen+1] = NULL;
        return !msh_bi_test(nlen + 1, nargv);
    }

    /* unary op */
    if (end - start >= 2 && is_unary(argv[start])) {
        return eval_unary(argv, &start);
    }
    /* binary op */
    if (end - start >= 3 && is_binary(argv[start + 1])) {
        return eval_binary(argv, &start);
    }
    /* single arg = true if non-empty */
    if (end - start == 1) return argv[start][0] == '\0';
    return 1;
}