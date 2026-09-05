/*
 * Msh - builtin: source (read and execute commands from file).
 */
#include "msh/builtin.h"
#include "msh/util.h"
#include "msh/lexer.h"
#include "msh/parser.h"
#include "msh/exec.h"

static int exec_line(const char *line)
{
    msh_lexer_t lx;
    msh_lexer_init(&lx, line);
    if (msh_lexer_tokenize(&lx) != 0) { msh_lexer_free(&lx); return 1; }
    msh_ast_t *ast = msh_parse(&lx);
    int rc = 0;
    if (ast) { rc = msh_exec_ast(ast); msh_ast_free(ast); }
    msh_lexer_free(&lx);
    return rc;
}

int msh_bi_source(int argc, char **argv)
{
    if (argc < 2) { msh_error("source: usage: source FILE"); return 1; }
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { msh_perror(argv[1]); return 1; }
    char buf[MSH_MAX_LINE];
    ssize_t n;
    while ((n = msh_read_line(fd, buf, sizeof(buf))) > 0) {
        char *line = msh_strtrim(buf);
        if (!*line || *line == '#') continue;
        int rc = exec_line(line);
        if (rc != 0) { close(fd); return rc; }
    }
    close(fd);
    return 0;
}