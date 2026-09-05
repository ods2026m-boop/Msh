/*
 * Msh - Config loading (e.g., ~/.mshrc, ~/.msh_aliases).
 */
#include "msh/config.h"
#include "msh/lexer.h"
#include "msh/parser.h"
#include "msh/exec.h"
#include "msh/util.h"
#include <fcntl.h>

void msh_config_load(const char *path)
{
    if (!path) return;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    char buf[MSH_MAX_LINE];
    ssize_t n;
    while ((n = msh_read_line(fd, buf, sizeof(buf))) > 0) {
        char *line = msh_strtrim(buf);
        if (!*line || *line == '#') continue;
        msh_lexer_t lx;
        msh_lexer_init(&lx, line);
        if (msh_lexer_tokenize(&lx) == 0) {
            msh_ast_t *ast = msh_parse(&lx);
            if (ast) {
                extern int msh_exec_ast(msh_ast_t *ast);
                msh_exec_ast(ast);
                msh_ast_free(ast);
            }
        }
        msh_lexer_free(&lx);
    }
    close(fd);
}