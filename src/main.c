/*
 * Msh - Main loop.
 */
#include "msh/msh.h"
#include "msh/util.h"
#include "msh/lexer.h"
#include "msh/parser.h"
#include "msh/exec.h"
#include "msh/prompt.h"
#include "msh/history.h"
#include "msh/job.h"
#include "msh/signal.h"
#include "msh/config.h"
#include "msh/var.h"
#include "msh/compound.h"
#include "msh/function.h"
#include <string.h>

static void load_rc_files(void)
{
    const char *home = getenv("HOME");
    if (!home) return;
    char path[MSH_MAX_LINE];
    snprintf(path, sizeof(path), "%s/.mshrc", home);
    msh_config_load(path);
    snprintf(path, sizeof(path), "%s/.msh_aliases", home);
    msh_config_load(path);
    snprintf(path, sizeof(path), "%s/.msh_history", home);
    msh_history_load(path);
}

static void save_history(void)
{
    const char *home = getenv("HOME");
    if (!home) return;
    char path[MSH_MAX_LINE];
    snprintf(path, sizeof(path), "%s/.msh_history", home);
    msh_history_save(path);
}

static int process_line(const char *line)
{
    if (msh_get_state()->debug) {
        fprintf(stderr, "PROCESS_LINE: %s\n", line);
    }
    if (!line || !*line) return 0;
    if (*line == '#') return 0;
    char *tmp = msh_strdup(line);
    char *trim = msh_strtrim(tmp);

    /* Compound command detection */
    const char *kw = NULL, *term = NULL;
    if (msh_strstarts(trim, "if "))    { kw = "if";    term = "fi"; }
    else if (msh_strstarts(trim, "while ")) { kw = "while"; term = "done"; }
    else if (msh_strstarts(trim, "for "))   { kw = "for";   term = "done"; }
    else if (msh_streq(trim, "{"))     { kw = "{";     term = "}"; }
    else if (trim[0] == '{' && trim[strlen(trim)-1] == '}') {
        /* one-line { ... } brace block — only if not a function def like "name() { ... }" */
        const char *paren = strchr(trim, '(');
        if (!paren || paren > strrchr(trim, '{')) { kw = "{"; term = "}"; }
    }
    else if (trim[0] != '\0') {
        const char *paren = strstr(trim, "()");
        if (paren && paren > trim) {
            const char *after_paren = paren + 2;
            while (*after_paren == ' ' || *after_paren == '\t') after_paren++;
            if (*after_paren == '{') {
                size_t namelen = (size_t)(paren - trim);
                char name[MSH_MAX_ARGS];
                if (namelen < sizeof(name)) {
                    memcpy(name, trim, namelen);
                    name[namelen] = '\0';
                    char *tname = msh_strtrim(name);
                    if (*tname) {
                        kw = "function_def";
                        term = "}";
                    }
                }
            }
        }
    }

    if (kw) {
        if (kw && msh_streq(kw, "function_def")) {
            const char *paren = strstr(trim, "()");
            size_t namelen = (size_t)(paren - trim);
            char name[MSH_MAX_ARGS];
            memcpy(name, trim, namelen);
            name[namelen] = '\0';
            char *fname = msh_strtrim(name);
            const char *rest = paren + 2;
            while (*rest == ' ' || *rest == '\t') rest++;
            if (*rest != '{') {
                fprintf(stderr, "msh: syntax error: expected '{' after function name\n");
                free(tmp);
                return 1;
            }
            rest++; /* skip '{' */
            while (*rest == ' ' || *rest == '\t') rest++;
            
            /* Find the closing '}' for the function body */
            const char *body_start = rest;
            const char *close = strchr(body_start, '}');
            size_t body_len = 0;
            if (close) {
                body_len = (size_t)(close - body_start);
            } else {
                /* Multi-line function: read until '}' from stdin */
                size_t init_len = strlen(body_start);
                char *multi = msh_xmalloc(init_len + 256);
                size_t cap = init_len + 256, n = init_len;
                memcpy(multi, body_start, init_len);
                char line[MSH_MAX_LINE];
                while (fgets(line, sizeof(line), stdin)) {
                    size_t l = strlen(line);
                    if (n + l + 2 >= cap) { cap = (n + l + 2) * 2; multi = msh_xrealloc(multi, cap); }
                    memcpy(multi + n, line, l); n += l;
                    multi[n] = '\0';
                    const char *ct = strrchr(multi, '}');
                    if (ct) {
                        char cn = ct[1];
                        if (cn == '\0' || cn == ' ' || cn == '\t' || cn == ';' || cn == '\n') {
                            close = ct;
                            body_len = (size_t)(ct - multi);
                            break;
                        }
                    }
                }
                free(multi);
            }
            
            char *body = msh_xmalloc(body_len + 1);
            if (close) {
                memcpy(body, body_start, body_len);
            }
            body[body_len] = '\0';
            char *trimmed_body = msh_strtrim(body);
            msh_function_define(fname, trimmed_body);
            free(body);
            
            /* Process rest after '}' */
            const char *after = close ? close + 1 : "";
            while (*after == ' ' || *after == '\t' || *after == ';') after++;
            if (*after) {
                msh_history_add(trim);
                int rc = process_line(after);
                msh_get_state()->last_status = rc;
                free(tmp);
                return rc;
            }
            msh_history_add(trim);
            free(tmp);
            return 0;
        }

        /* body = remainder after keyword; if terminator already on same line,
         * skip the stdin-reading phase. */
        const char *rest = trim;
        if (*rest == '{') rest++;
        else {
            while (*rest && *rest != ' ' && *rest != '\t') rest++;
            while (*rest == ' ' || *rest == '\t') rest++;
        }
        size_t init_len = strlen(rest);
        char *body = msh_xmalloc(init_len + 256);
        size_t cap = init_len + 256, n = 0;
        memcpy(body, rest, init_len); n = init_len;
        body[n++] = '\n'; body[n] = '\0';
        /* check whether the initial line already contains terminator */
        int has_term = 0;
        if (term) {
            const char *t = strstr(rest, term);
            if (t) {
                char next = t[strlen(term)];
                if (next == ' ' || next == '\t' || next == '\n' || next == ';' || next == '\0')
                    has_term = 1;
            }
        }
        if (!has_term) {
            char line[MSH_MAX_LINE];
            while (fgets(line, sizeof(line), stdin)) {
                char *t = msh_strtrim(line);
                int is_term = (term && msh_streq(t, term));
                size_t l = strlen(line);
                if (n + l + 2 >= cap) { cap = (n + l + 2) * 2; body = msh_xrealloc(body, cap); }
                memcpy(body + n, line, l); n += l;
                body[n++] = '\n'; body[n] = '\0';
                if (is_term) break;
            }
        }
        msh_history_add(trim);
        int rc = msh_compound_dispatch(kw, body);
        free(body);
        msh_get_state()->last_status = rc;
        free(tmp);
        return rc;
    }

    /* Short-circuit logical operators: && and || outside of compound commands.
     * Split the line at top-level &&/|| and execute each segment in turn. */
    {
        int depth = 0;
        int op_start = -1, op_kind = 0; /* 1=&&, 2=|| */
        for (int k = 0; trim[k]; k++) {
            char c = trim[k];
            if (c == '"' || c == '\'') { /* skip quoted */ }
            else if (c == '(') depth++;
            else if (c == ')') depth--;
            else if (depth == 0 && c == '&' && trim[k+1] == '&' &&
                     (k == 0 || trim[k-1] != '&')) {
                op_start = k; op_kind = 1; break;
            }
            else if (depth == 0 && c == '|' && trim[k+1] == '|' &&
                     (k == 0 || trim[k-1] != '|')) {
                op_start = k; op_kind = 2; break;
            }
        }
        if (op_start >= 0) {
            char *left  = msh_strndup(trim, op_start);
            char *right = msh_strdup(trim + op_start + 2);
            char *lt = msh_strtrim(left);
            char *rt = msh_strtrim(right);
            int rc;
            if (op_kind == 1) { /* && */
                rc = process_line(lt);
                if (rc == 0) rc = process_line(rt);
            } else { /* || */
                rc = process_line(lt);
                if (rc != 0) rc = process_line(rt);
                else rc = 0;
            }
            free(left); free(right);
            return rc;
        }
    }

    msh_lexer_t lx;
    msh_lexer_init(&lx, line);
    if (msh_lexer_tokenize(&lx) != 0) { msh_lexer_free(&lx); free(tmp); return 0; }
    msh_ast_t *ast = msh_parse(&lx);
    int rc = 0;
    if (ast) {
        msh_history_add(line);
        rc = msh_exec_ast(ast);
        msh_ast_free(ast);
    }
    msh_lexer_free(&lx);
    free(tmp);
    return rc;
}

static void shutdown_jobs(void)
{
    msh_state_t *st = msh_get_state();
    for (msh_job_t *j = st->jobs_head; j; j = j->next)
        kill(-j->pgid, SIGHUP);
}

int main(int argc, char **argv)
{
    int opt_c = 0;
    char *command = NULL;
    int debug_flag = 0;
    /* Parse command line options */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 < argc) {
                opt_c = 1;
                command = argv[i+1];
                i++; /* skip the argument */
            } else {
                fprintf(stderr, "msh: -c requires an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-x") == 0 || strcmp(argv[i], "--debug") == 0) {
            debug_flag = 1;
        }
        /* Ignore other options for now */
    }

    msh_state_init();
    msh_signal_init();
    msh_history_init();
    load_rc_files();

    if (debug_flag || getenv("MSH_DEBUG")) {
        msh_get_state()->debug = 1;
    }

    if (opt_c) {
        int rc = process_line(command);
        msh_state_t *st = msh_get_state();
        if (st->should_exit) rc = st->exit_code;
        shutdown_jobs();
        save_history();
        putchar('\n');
        msh_state_cleanup();
        return rc;
    }

    char buf[MSH_MAX_LINE];
    int last_rc = 0;
    while (1) {
        msh_signal_dispatch();
        int n = msh_prompt_input(buf, sizeof(buf));
        if (n < 0) break;
        char *line = msh_strtrim(buf);
        if (!*line) continue;
        last_rc = process_line(line);
        msh_state_t *st = msh_get_state();
        if (st->should_exit) {
            last_rc = st->exit_code;
            break;
        }
        st->last_status = last_rc;
    }
    shutdown_jobs();
    save_history();
    putchar('\n');
    msh_state_cleanup();
    msh_state_t *st = msh_get_state();
    return st->exit_code;
}