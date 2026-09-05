/*
 * Msh - History (in-memory; persistent save optional).
 */
#include "msh/history.h"
#include "msh/util.h"
#include <fcntl.h>

void msh_history_init(void) { /* state already initialized */ }

void msh_history_add(const char *line)
{
    if (!line || !*line) return;
    msh_state_t *st = msh_get_state();
    /* skip duplicates of last entry */
    if (st->history_count > 0) {
        char *last = st->history[st->history_count - 1].line;
        if (last && msh_streq(last, line)) return;
    }
    if (st->history_count >= st->history_capacity) {
        /* drop oldest */
        free(st->history[0].line);
        memmove(&st->history[0], &st->history[1],
                (st->history_capacity - 1) * sizeof(msh_hist_entry_t));
        st->history_count--;
    }
    msh_hist_entry_t *e = &st->history[st->history_count++];
    e->line = msh_strdup(line);
    e->index = st->history_count;
    st->history_cursor = st->history_count;
}

char *msh_history_get(int index)
{
    msh_state_t *st = msh_get_state();
    if (index < 0 || index >= st->history_count) return NULL;
    return st->history[index].line;
}

int msh_history_size(void)
{
    msh_state_t *st = msh_get_state();
    return st->history_count;
}

void msh_history_clear(void)
{
    msh_state_t *st = msh_get_state();
    for (int i = 0; i < st->history_count; i++) free(st->history[i].line);
    st->history_count = 0;
    st->history_cursor = 0;
}

void msh_history_delete(int index)
{
    msh_state_t *st = msh_get_state();
    if (index < 0 || index >= st->history_count) {
        msh_error("history: %d: invalid history position", index + 1);
        return;
    }
    free(st->history[index].line);
    memmove(&st->history[index], &st->history[index+1],
            (st->history_count - index - 1) * sizeof(msh_hist_entry_t));
    st->history_count--;
    if (st->history_cursor > st->history_count)
        st->history_cursor = st->history_count;
}

const char *msh_history_prev(const char *current)
{
    (void)current;
    msh_state_t *st = msh_get_state();
    if (st->history_cursor <= 0) return NULL;
    st->history_cursor--;
    return st->history[st->history_cursor].line;
}

const char *msh_history_next(const char *current)
{
    (void)current;
    msh_state_t *st = msh_get_state();
    if (st->history_cursor >= st->history_count - 1) {
        st->history_cursor = st->history_count;
        return "";
    }
    st->history_cursor++;
    return st->history[st->history_cursor].line;
}

void msh_history_load(const char *path)
{
    if (!path) return;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    char buf[MSH_MAX_LINE];
    ssize_t n;
    while ((n = msh_read_line(fd, buf, sizeof(buf))) > 0) {
        char *p = msh_strtrim(buf);
        if (*p) msh_history_add(p);
    }
    close(fd);
}

void msh_history_save(const char *path)
{
    if (!path) return;
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return;
    msh_state_t *st = msh_get_state();
    for (int i = 0; i < st->history_count; i++) {
        msh_write_all(fd, st->history[i].line, strlen(st->history[i].line));
        msh_write_all(fd, "\n", 1);
    }
    close(fd);
}