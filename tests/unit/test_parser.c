#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msh/lexer.h"
#include "msh/parser.h"
#include "msh/util.h"

static int test_simple_command_parse(void)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, "echo hello world");
    if (msh_lexer_tokenize(&lx) != 0) {
        fprintf(stderr, "FAIL: tokenization error\n");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_ast_t *ast = msh_parse(&lx);
    if (!ast) {
        fprintf(stderr, "FAIL: parse returned NULL\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (!ast->head || ast->head->ncmds != 1) {
        fprintf(stderr, "FAIL: expected 1 command in pipeline\n");
        msh_ast_free(ast);
        msh_lexer_free(&lx);
        return 1;
    }
    msh_cmd_t *cmd = &ast->head->cmds[0];
    if (cmd->argc != 3 || strcmp(cmd->argv[0], "echo") != 0 ||
        strcmp(cmd->argv[1], "hello") != 0 || strcmp(cmd->argv[2], "world") != 0) {
        fprintf(stderr, "FAIL: command argv mismatch\n");
        msh_ast_free(ast);
        msh_lexer_free(&lx);
        return 1;
    }
    msh_ast_free(ast);
    msh_lexer_free(&lx);
    return 0;
}

static int test_pipeline_parse(void)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, "cat file | grep hello");
    if (msh_lexer_tokenize(&lx) != 0) {
        fprintf(stderr, "FAIL: tokenization error\n");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_ast_t *ast = msh_parse(&lx);
    if (!ast) {
        fprintf(stderr, "FAIL: parse returned NULL\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (!ast->head || ast->head->ncmds != 2) {
        fprintf(stderr, "FAIL: expected 2 commands in pipeline, got %d\n",
                ast->head ? ast->head->ncmds : 0);
        msh_ast_free(ast);
        msh_lexer_free(&lx);
        return 1;
    }
    msh_ast_free(ast);
    msh_lexer_free(&lx);
    return 0;
}

static int test_function_def_tokens_parse(void)
{
    /* The parser should accept function-def syntax as regular tokens;
     * actual function registration happens in main.c before parsing. */
    msh_lexer_t lx;
    msh_lexer_init(&lx, "myfunc() { echo hi; }");
    if (msh_lexer_tokenize(&lx) != 0) {
        fprintf(stderr, "FAIL: tokenization error\n");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_ast_t *ast = msh_parse(&lx);
    if (!ast) {
        fprintf(stderr, "FAIL: parse returned NULL for function def\n");
        msh_lexer_free(&lx);
        return 1;
    }
    if (!ast->head || ast->head->ncmds < 1) {
        fprintf(stderr, "FAIL: expected at least 1 command\n");
        msh_ast_free(ast);
        msh_lexer_free(&lx);
        return 1;
    }
    msh_cmd_t *cmd = &ast->head->cmds[0];
    if (cmd->argc < 1 || strcmp(cmd->argv[0], "myfunc") != 0) {
        fprintf(stderr, "FAIL: expected first arg 'myfunc'\n");
        msh_ast_free(ast);
        msh_lexer_free(&lx);
        return 1;
    }
    msh_ast_free(ast);
    msh_lexer_free(&lx);
    return 0;
}

static int test_quoted_string_in_parse(void)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, "echo 'hello world'");
    if (msh_lexer_tokenize(&lx) != 0) {
        fprintf(stderr, "FAIL: tokenization error\n");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_ast_t *ast = msh_parse(&lx);
    if (!ast) {
        fprintf(stderr, "FAIL: parse returned NULL\n");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_cmd_t *cmd = &ast->head->cmds[0];
    if (cmd->argc != 2) {
        fprintf(stderr, "FAIL: expected 2 args, got %d\n", cmd->argc);
        msh_ast_free(ast);
        msh_lexer_free(&lx);
        return 1;
    }
    if (strcmp(cmd->argv[1], "'hello world'") != 0) {
        fprintf(stderr, "FAIL: expected single-quoted string with markers, got '%s'\n", cmd->argv[1]);
        msh_ast_free(ast);
        msh_lexer_free(&lx);
        return 1;
    }
    msh_ast_free(ast);
    msh_lexer_free(&lx);
    return 0;
}

static int test_cmdsub_in_parse(void)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, "echo $(echo hi)");
    if (msh_lexer_tokenize(&lx) != 0) {
        fprintf(stderr, "FAIL: tokenization error\n");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_ast_t *ast = msh_parse(&lx);
    if (!ast) {
        fprintf(stderr, "FAIL: parse returned NULL\n");
        msh_lexer_free(&lx);
        return 1;
    }
    msh_cmd_t *cmd = &ast->head->cmds[0];
    if (cmd->argc != 2) {
        fprintf(stderr, "FAIL: expected 2 args, got %d\n", cmd->argc);
        msh_ast_free(ast);
        msh_lexer_free(&lx);
        return 1;
    }
    if (strcmp(cmd->argv[1], "$(echo hi)") != 0) {
        fprintf(stderr, "FAIL: expected cmdsub token, got '%s'\n", cmd->argv[1]);
        msh_ast_free(ast);
        msh_lexer_free(&lx);
        return 1;
    }
    msh_ast_free(ast);
    msh_lexer_free(&lx);
    return 0;
}

int main(void)
{
    int fails = 0;
    if (test_simple_command_parse()) { fprintf(stderr, "FAIL: simple_command_parse\n"); fails++; }
    if (test_pipeline_parse()) { fprintf(stderr, "FAIL: pipeline_parse\n"); fails++; }
    if (test_function_def_tokens_parse()) { fprintf(stderr, "FAIL: function_def_tokens_parse\n"); fails++; }
    if (test_quoted_string_in_parse()) { fprintf(stderr, "FAIL: quoted_string_in_parse\n"); fails++; }
    if (test_cmdsub_in_parse()) { fprintf(stderr, "FAIL: cmdsub_in_parse\n"); fails++; }

    if (fails == 0) {
        printf("PASS: all %d parser tests passed\n", 5);
        return 0;
    }
    fprintf(stderr, "FAIL: %d parser test(s) failed\n", fails);
    return 1;
}
