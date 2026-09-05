/*
 * Msh - Redirections
 * Applies redirection descriptors: <, >, >>, 2>, 2>>, <<.
 */
#include "msh/redir.h"
#include "msh/util.h"
#include <fcntl.h>

int msh_open_redir(msh_redir_t *r)
{
    if (!r) return 0;
    int fd = -1;
    switch (r->type) {
        case REDIR_IN:
            fd = open(r->target, O_RDONLY);
            if (fd < 0) { msh_perror(r->target); return -1; }
            if (dup2(fd, 0) < 0) { msh_perror("dup2"); close(fd); return -1; }
            close(fd);
            break;
        case REDIR_OUT:
            fd = open(r->target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { msh_perror(r->target); return -1; }
            if (dup2(fd, 1) < 0) { msh_perror("dup2"); close(fd); return -1; }
            close(fd);
            break;
        case REDIR_APPEND:
            fd = open(r->target, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) { msh_perror(r->target); return -1; }
            if (dup2(fd, 1) < 0) { msh_perror("dup2"); close(fd); return -1; }
            close(fd);
            break;
        case REDIR_ERR:
            fd = open(r->target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { msh_perror(r->target); return -1; }
            if (dup2(fd, 2) < 0) { msh_perror("dup2"); close(fd); return -1; }
            close(fd);
            break;
        case REDIR_ERR_APP:
            fd = open(r->target, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) { msh_perror(r->target); return -1; }
            if (dup2(fd, 2) < 0) { msh_perror("dup2"); close(fd); return -1; }
            close(fd);
            break;
        case REDIR_BOTH:
            fd = open(r->target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { msh_perror(r->target); return -1; }
            if (dup2(fd, 1) < 0 || dup2(fd, 2) < 0) { msh_perror("dup2"); close(fd); return -1; }
            close(fd);
            break;
        case REDIR_BOTH_APP:
            fd = open(r->target, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) { msh_perror(r->target); return -1; }
            if (dup2(fd, 1) < 0 || dup2(fd, 2) < 0) { msh_perror("dup2"); close(fd); return -1; }
            close(fd);
            break;
        case REDIR_HEREDOC: {
            /* read from target until delimiter line; for simplicity
             * we treat the heredoc target as a literal filename. */
            fd = open(r->target, O_RDONLY);
            if (fd < 0) { msh_perror(r->target); return -1; }
            if (dup2(fd, 0) < 0) { msh_perror("dup2"); close(fd); return -1; }
            close(fd);
            break;
        }
        default: break;
    }
    return 0;
}