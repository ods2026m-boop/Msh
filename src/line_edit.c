/*
 * Msh - Line editor (raw terminal mode, no readline).
 *
 * Features:
 *   - Insert/delete characters at cursor
 *   - Left/Right arrow to move cursor
 *   - Home/End to jump to start/end
 *   - Backspace to delete char before cursor
 *   - Ctrl-D at empty line -> EOF
 *   - Ctrl-C -> clear line, redraw prompt
 *   - Up/Down -> history navigation
 *   - Tab -> completion
 *   - Ctrl-R -> reverse incremental history search (best-effort)
 */
#include "msh/line_edit.h"
#include "msh/util.h"
#include "msh/history.h"
#include "msh/completion.h"
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

static struct termios g_old_tio;

static int tio_enable_raw(void)
{
    if (!isatty(STDIN_FILENO)) return 0;
    if (tcgetattr(STDIN_FILENO, &g_old_tio) < 0) return -1;
    struct termios tio = g_old_tio;
    tio.c_lflag &= ~(ICANON | ECHO | ISIG);
    tio.c_iflag &= ~(IXON | ICRNL);
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &tio) < 0) return -1;
    return 0;
}

static void tio_restore(void)
{
    if (isatty(STDIN_FILENO))
        tcsetattr(STDIN_FILENO, TCSANOW, &g_old_tio);
}

static void le_clear_line(const char *prompt)
{
    /* move to start, clear entire line, redraw prompt + buffer */
    fputs("\r\033[2K", stdout);
    fputs(prompt, stdout);
    fflush(stdout);
}

static void le_redraw(const char *prompt, const char *buf, int len, int cur)
{
    fputs("\r\033[2K", stdout);
    fputs(prompt, stdout);
    fwrite(buf, 1, len, stdout);
    if (cur < len) printf("\033[%dD", len - cur);
    fflush(stdout);
}

static int le_insert(char *buf, int max, int *len, int *cur, char c)
{
    if (*len + 1 >= max) return 0;
    memmove(buf + *cur + 1, buf + *cur, *len - *cur + 1);
    buf[(*cur)++] = c;
    (*len)++;
    return 1;
}

static void le_delete_at(char *buf, int *len, int *cur)
{
    if (*cur >= *len) return;
    memmove(buf + *cur, buf + *cur + 1, *len - *cur);
    (*len)--;
}

static void le_backspace(char *buf, int *len, int *cur)
{
    if (*cur == 0) return;
    memmove(buf + *cur - 1, buf + *cur, *len - *cur + 1);
    (*cur)--; (*len)--;
}

static int le_read_byte(void)
{
    unsigned char c;
    ssize_t n;
    do { n = read(STDIN_FILENO, &c, 1); } while (n < 0 && errno == EINTR);
    return n <= 0 ? -1 : (int)c;
}

/* Reverse incremental search.
 * Returns final selected line (caller must free), or NULL if cancelled. */
static char *le_reverse_search(const char *prompt, char *buf, int *cur_out, int *len_out)
{
    char query[128];
    int qlen = 0; query[0] = '\0';
    int found = 0;
    char *result = NULL;

    le_clear_line(prompt);
    fputs("(reverse-i-search)`': ", stdout); fflush(stdout);

    while (1) {
        int c = le_read_byte();
        if (c < 0) break;
        if (c == '\n' || c == '\r') break;
        if (c == 27) { /* ESC */
            int b = le_read_byte();
            if (b == '[' || b == 'O') {
                int d = le_read_byte();
                (void)d;
            }
            break;
        }
        if (c == 8 || c == 127) { /* backspace */
            if (qlen == 0) break;
            qlen--;
            query[qlen] = '\0';
        } else if (c == 12) { /* Ctrl-L: refresh */
            /* nothing */
        } else if (c == 3 || c == 7) { /* Ctrl-C / Ctrl-G: abort */
            found = 0; break;
        } else if (c >= 32 && c < 127 && qlen + 1 < (int)sizeof(query)) {
            query[qlen++] = (char)c;
            query[qlen] = '\0';
        } else continue;

        /* search from end */
        int n = msh_history_size();
        found = 0;
        for (int i = n - 1; i >= 0; i--) {
            char *line = msh_history_get(i);
            if (!line) continue;
            if (strstr(line, query)) {
                /* copy to buf */
                int len = (int)strlen(line);
                if (len >= MSH_MAX_LINE) len = MSH_MAX_LINE - 1;
                memcpy(buf, line, len);
                buf[len] = '\0';
                *len_out = len;
                *cur_out = len;
                result = msh_strdup(line);
                found = 1;
                break;
            }
        }
        /* redraw: prompt + query + match */
        fputs("\r\033[2K", stdout);
        printf("(reverse-i-search)`%s': %s", query, found ? buf : "");
        fflush(stdout);
    }
    if (!found) result = NULL;
    return result;
}

int msh_line_edit_read(char *buf, size_t max, const char *prompt)
{
    if (!isatty(STDIN_FILENO)) {
        if (!fgets(buf, max, stdin)) return -1;
        size_t l = strlen(buf);
        if (l && buf[l-1] == '\n') buf[l-1] = '\0';
        return (int)l;
    }
    if (tio_enable_raw() < 0) return -1;

    int len = 0, cur = 0;
    buf[0] = '\0';
    fputs(prompt, stdout); fflush(stdout);

    int rc = (int)max - 1;
    while (1) {
        int c = le_read_byte();
        if (c < 0) { rc = -1; break; }
        if (c == '\n' || c == '\r') { putchar('\n'); break; }
        if (c == 4) { /* Ctrl-D */
            if (len == 0) { rc = -1; break; }
            le_delete_at(buf, &len, &cur);
        } else if (c == 3) { /* Ctrl-C: clear line */
            len = 0; cur = 0; buf[0] = '\0';
            putchar('\n');
        } else if (c == 12) { /* Ctrl-L: redraw */
            le_redraw(prompt, buf, len, cur);
        } else if (c == 8 || c == 127) { /* backspace */
            le_backspace(buf, &len, &cur);
        } else if (c == 18) { /* Ctrl-R: reverse search */
            char *res = le_reverse_search(prompt, buf, &cur, &len);
            if (res) { free(res); }
        } else if (c == 27) { /* ESC sequence */
            int b = le_read_byte();
            if (b != '[' && b != 'O') continue;
            int d = le_read_byte();
            switch (d) {
                case 'A': { /* Up */
                    const char *h = msh_history_prev(buf);
                    if (h) { snprintf(buf, max, "%s", h); len = cur = (int)strlen(buf); }
                    break;
                }
                case 'B': { /* Down */
                    const char *h = msh_history_next(buf);
                    if (h) { snprintf(buf, max, "%s", h); len = cur = (int)strlen(buf); }
                    break;
                }
                case 'C': if (cur < len) cur++; break;
                case 'D': if (cur > 0) cur--; break;
                case 'H': cur = 0; break;       /* Home */
                case 'F': cur = len; break;      /* End */
                case '1': { /* possibly Home with modifier */
                    int e = le_read_byte();
                    if (e == '~') cur = 0;
                    break;
                }
                case '4': { /* possibly End */
                    int e = le_read_byte();
                    if (e == '~') cur = len;
                    break;
                }
                case '3': { /* Delete key */
                    int e = le_read_byte();
                    if (e == '~') le_delete_at(buf, &len, &cur);
                    break;
                }
            }
        } else if (c == '\t') {
            msh_completions_t c0;
            msh_complete(buf, cur, &c0);
            if (c0.count == 1) {
                /* append remainder of completion */
                const char *start = buf;
                for (int i = cur - 1; i >= 0; i--) {
                    if (msh_isspace((unsigned char)buf[i])) { start = &buf[i+1]; break; }
                }
                size_t off = start - buf;
                const char *comp = c0.items[0];
                const char *bslash = strrchr(comp, '/');
                const char *word = bslash ? bslash + 1 : comp;
                const char *suffix = word + (cur - off);
                while (*suffix && (size_t)len + 1 < max) {
                    le_insert(buf, (int)max, &len, &cur, *suffix++);
                }
                /* add a space or trailing slash for directories */
                if ((size_t)len + 1 < max) {
                    le_insert(buf, (int)max, &len, &cur, ' ');
                }
            } else if (c0.count > 1) {
                /* find common prefix to extend line, then list */
                char *cp = msh_completion_common_prefix(&c0);
                size_t cplen = strlen(cp);
                const char *start = buf;
                for (int i = cur - 1; i >= 0; i--) {
                    if (msh_isspace((unsigned char)buf[i])) { start = &buf[i+1]; break; }
                }
                size_t off = start - buf;
                /* extend with common prefix beyond what was already typed */
                const char *ref = c0.items[0];
                const char *bslash = strrchr(ref, '/');
                const char *word = bslash ? bslash + 1 : ref;
                size_t word_len = strlen(word);
                if (cplen > word_len - (cur - off)) {
                    const char *suffix = word + (cur - off);
                    const char *end = word + cplen;
                    while (suffix < end && (size_t)len + 1 < max) {
                        le_insert(buf, (int)max, &len, &cur, *suffix++);
                    }
                }
                putchar('\n');
                /* print in columns */
                size_t maxw = 0;
                for (int i = 0; i < c0.count; i++) {
                    size_t l = strlen(c0.items[i]);
                    if (l > maxw) maxw = l;
                }
                int cols = 80 / (maxw + 2);
                if (cols < 1) cols = 1;
                for (int i = 0; i < c0.count; i++) {
                    fputs(c0.items[i], stdout);
                    if ((i + 1) % cols == 0 || i == c0.count - 1) putchar('\n');
                    else {
                        for (size_t k = strlen(c0.items[i]); k < maxw + 2; k++) putchar(' ');
                    }
                }
                free(cp);
            }
            msh_completion_free(&c0);
        } else if (c >= 32 && c < 127) {
            le_insert(buf, (int)max, &len, &cur, (char)c);
        }
        le_redraw(prompt, buf, len, cur);
    }

    tio_restore();
    if (rc == -1) return -1;
    return len;
}