#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msh/expand.h"
#include "msh/util.h"

static int test_simple_var_expansion(void)
{
    setenv("MYVAR", "hello", 1);
    char *r = msh_expand_vars("$MYVAR");
    if (!r || strcmp(r, "hello") != 0) {
        fprintf(stderr, "FAIL: simple var expansion: got '%s'\n", r ? r : "(null)");
        free(r);
        return 1;
    }
    free(r);
    return 0;
}

static int test_arithmetic_expansion(void)
{
    char *r = msh_expand_vars("$((2+3))");
    if (!r || strcmp(r, "5") != 0) {
        fprintf(stderr, "FAIL: arithmetic expansion: got '%s'\n", r ? r : "(null)");
        free(r);
        return 1;
    }
    free(r);
    return 0;
}

static int test_arithmetic_in_string(void)
{
    char *r = msh_expand_vars("result=$((10*2))");
    if (!r || strcmp(r, "result=20") != 0) {
        fprintf(stderr, "FAIL: arithmetic in string: got '%s'\n", r ? r : "(null)");
        free(r);
        return 1;
    }
    free(r);
    return 0;
}

static int test_single_quoted_no_expansion(void)
{
    setenv("MYVAR", "expanded", 1);
    char *r = msh_expand_vars("'$MYVAR'");
    if (!r || strcmp(r, "$MYVAR") != 0) {
        fprintf(stderr, "FAIL: single-quoted no expansion: got '%s'\n", r ? r : "(null)");
        free(r);
        return 1;
    }
    free(r);
    return 0;
}

static int test_double_quoted_with_expansion(void)
{
    setenv("MYVAR", "world", 1);
    char *r = msh_expand_vars("hello $MYVAR");
    if (!r || strcmp(r, "hello world") != 0) {
        fprintf(stderr, "FAIL: double-quoted expansion: got '%s'\n", r ? r : "(null)");
        free(r);
        return 1;
    }
    free(r);
    return 0;
}

static int test_tilde_expansion(void)
{
    setenv("HOME", "/home/test", 1);
    char *r = msh_expand_vars("~/docs");
    if (!r || strcmp(r, "/home/test/docs") != 0) {
        fprintf(stderr, "FAIL: tilde expansion: got '%s'\n", r ? r : "(null)");
        free(r);
        return 1;
    }
    free(r);
    return 0;
}

static int test_special_vars(void)
{
    char *r = msh_expand_vars("$?");
    if (!r || strcmp(r, "0") != 0) {
        fprintf(stderr, "FAIL: special var $?: got '%s'\n", r ? r : "(null)");
        free(r);
        return 1;
    }
    free(r);
    return 0;
}

int main(void)
{
    int fails = 0;
    if (test_simple_var_expansion()) { fprintf(stderr, "FAIL: simple_var_expansion\n"); fails++; }
    if (test_arithmetic_expansion()) { fprintf(stderr, "FAIL: arithmetic_expansion\n"); fails++; }
    if (test_arithmetic_in_string()) { fprintf(stderr, "FAIL: arithmetic_in_string\n"); fails++; }
    if (test_single_quoted_no_expansion()) { fprintf(stderr, "FAIL: single_quoted_no_expansion\n"); fails++; }
    if (test_double_quoted_with_expansion()) { fprintf(stderr, "FAIL: double_quoted_with_expansion\n"); fails++; }
    if (test_tilde_expansion()) { fprintf(stderr, "FAIL: tilde_expansion\n"); fails++; }
    if (test_special_vars()) { fprintf(stderr, "FAIL: special_vars\n"); fails++; }

    if (fails == 0) {
        printf("PASS: all %d expand tests passed\n", 7);
        return 0;
    }
    fprintf(stderr, "FAIL: %d expand test(s) failed\n", fails);
    return 1;
}
