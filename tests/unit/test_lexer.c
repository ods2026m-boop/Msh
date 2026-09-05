#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msh/lexer.h"
#include "msh/util.h"

static int test_basic_tokenization(void)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, "echo hello world");
    if (msh_lexer_tokenize(&lx) != 0) {
        fprintf(stderr, "FAIL: basic tokenization returned error\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.ntok != 4) {
        fprintf(stderr, "FAIL: expected 4 tokens, got %d\n", lx.ntok);
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.tokens[0].type != TOK_WORD || strcmp(lx.tokens[0].value, "echo") != 0) {
        fprintf(stderr, "FAIL: token[0] mismatch\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.tokens[1].type != TOK_WORD || strcmp(lx.tokens[1].value, "hello") != 0) {
        fprintf(stderr, "FAIL: token[1] mismatch\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.tokens[2].type != TOK_WORD || strcmp(lx.tokens[2].value, "world") != 0) {
        fprintf(stderr, "FAIL: token[2] mismatch\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.tokens[3].type != TOK_EOF) {
        fprintf(stderr, "FAIL: expected EOF token\n");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_lexer_free(&lx);
    return 0;
}

static int test_double_quoted_string(void)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, "echo \"hello world\"");
    if (msh_lexer_tokenize(&lx) != 0) {
        fprintf(stderr, "FAIL: double-quote tokenization error\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.ntok != 3) {
        fprintf(stderr, "FAIL: expected 3 tokens for double-quote, got %d\n", lx.ntok);
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.tokens[1].type != TOK_STRING || strcmp(lx.tokens[1].value, "hello world") != 0) {
        fprintf(stderr, "FAIL: double-quoted string mismatch: got type=%d val=%s\n",
                lx.tokens[1].type, lx.tokens[1].value ? lx.tokens[1].value : "(null)");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_lexer_free(&lx);
    return 0;
}

static int test_single_quoted_string(void)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, "echo '$X'");
    if (msh_lexer_tokenize(&lx) != 0) {
        fprintf(stderr, "FAIL: single-quote tokenization error\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.ntok != 3) {
        fprintf(stderr, "FAIL: expected 3 tokens for single-quote, got %d\n", lx.ntok);
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.tokens[1].type != TOK_STRING) {
        fprintf(stderr, "FAIL: expected TOK_STRING for single-quote, got %d\n", lx.tokens[1].type);
        msh_lexer_free(&lx);
        return 1;
    }
    if (strcmp(lx.tokens[1].value, "'$X'") != 0) {
        fprintf(stderr, "FAIL: single-quoted string should preserve quotes: got '%s'\n",
                lx.tokens[1].value ? lx.tokens[1].value : "(null)");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_lexer_free(&lx);
    return 0;
}

static int test_arithmetic_tokenization(void)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, "echo $((2+3))");
    if (msh_lexer_tokenize(&lx) != 0) {
        fprintf(stderr, "FAIL: arithmetic tokenization error\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.ntok != 3) {
        fprintf(stderr, "FAIL: expected 3 tokens for arithmetic, got %d\n", lx.ntok);
        for (int i = 0; i < lx.ntok; i++)
            fprintf(stderr, "  token[%d]: type=%d val=%s\n", i, lx.tokens[i].type, lx.tokens[i].value);
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.tokens[1].type != TOK_WORD || strcmp(lx.tokens[1].value, "$((2+3))") != 0) {
        fprintf(stderr, "FAIL: arithmetic token mismatch: got type=%d val=%s\n",
                lx.tokens[1].type, lx.tokens[1].value ? lx.tokens[1].value : "(null)");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_lexer_free(&lx);
    return 0;
}

static int test_cmdsub_tokenization(void)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, "echo $(echo nested)");
    if (msh_lexer_tokenize(&lx) != 0) {
        fprintf(stderr, "FAIL: cmdsub tokenization error\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.ntok != 3) {
        fprintf(stderr, "FAIL: expected 3 tokens for cmdsub, got %d\n", lx.ntok);
        for (int i = 0; i < lx.ntok; i++)
            fprintf(stderr, "  token[%d]: type=%d val=%s\n", i, lx.tokens[i].type, lx.tokens[i].value);
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.tokens[1].type != TOK_CMDSUB || strcmp(lx.tokens[1].value, "$(echo nested)") != 0) {
        fprintf(stderr, "FAIL: cmdsub token mismatch: got type=%d val=%s\n",
                lx.tokens[1].type, lx.tokens[1].value ? lx.tokens[1].value : "(null)");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_lexer_free(&lx);
    return 0;
}

static int test_nested_arithmetic(void)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, "echo $((1+(2*3)))");
    if (msh_lexer_tokenize(&lx) != 0) {
        fprintf(stderr, "FAIL: nested arithmetic tokenization error\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.ntok != 3) {
        fprintf(stderr, "FAIL: expected 3 tokens for nested arithmetic, got %d\n", lx.ntok);
        msh_lexer_free(&lx);
        return 1;
    }
    if (lx.tokens[1].type != TOK_WORD || strcmp(lx.tokens[1].value, "$((1+(2*3)))") != 0) {
        fprintf(stderr, "FAIL: nested arithmetic token mismatch: got '%s'\n",
                lx.tokens[1].value ? lx.tokens[1].value : "(null)");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_lexer_free(&lx);
    return 0;
}

int main(void)
{
    int fails = 0;
    if (test_basic_tokenization()) { fprintf(stderr, "FAIL: basic_tokenization\n"); fails++; }
    if (test_double_quoted_string()) { fprintf(stderr, "FAIL: double_quoted_string\n"); fails++; }
    if (test_single_quoted_string()) { fprintf(stderr, "FAIL: single_quoted_string\n"); fails++; }
    if (test_arithmetic_tokenization()) { fprintf(stderr, "FAIL: arithmetic_tokenization\n"); fails++; }
    if (test_cmdsub_tokenization()) { fprintf(stderr, "FAIL: cmdsub_tokenization\n"); fails++; }
    if (test_nested_arithmetic()) { fprintf(stderr, "FAIL: nested_arithmetic\n"); fails++; }

    if (fails == 0) {
        printf("PASS: all %d lexer tests passed\n", 6);
        return 0;
    }
    fprintf(stderr, "FAIL: %d lexer test(s) failed\n", fails);
    return 1;
}
