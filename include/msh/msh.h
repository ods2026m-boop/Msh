/*
 * Msh - Monkey Shell
 * Main header file containing all core structures and constants.
 * C99 standard, no external dependencies.
 */
#ifndef MSH_H
#define MSH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>

/* Constants */
#define MSH_VERSION       "1.0.0"
#define MSH_NAME          "Msh"
#define MSH_MAX_LINE      4096
#define MSH_MAX_ARGS      256
#define MSH_MAX_TOKEN     1024
#define MSH_MAX_JOBS      64
#define MSH_MAX_HISTORY   1024
#define MSH_MAX_ALIASES   256
#define MSH_MAX_COMPLETIONS 256
#define MSH_MAX_PROMPT    512

/* Token types */
typedef enum {
    TOK_WORD,
    TOK_STRING,
    TOK_PIPE,        /* | */
    TOK_AND,         /* && */
    TOK_OR,          /* || */
    TOK_SEMI,        /* ; */
    TOK_BG,          /* & */
    TOK_REDIR_IN,    /* < */
    TOK_REDIR_OUT,   /* > */
    TOK_REDIR_APP,   /* >> */
    TOK_REDIR_ERR,   /* 2> */
    TOK_REDIR_ERR_APP,/* 2>> */
    TOK_REDIR_BOTH,  /* &> */
    TOK_REDIR_BOTH_APP, /* &>> */
    TOK_REDIR_HEREDOC, /* << */
    TOK_LPAREN,      /* ( */
    TOK_RPAREN,      /* ) */
    TOK_LBRACE,      /* { */
    TOK_RBRACE,      /* } */
    TOK_CMDSUB,      /* $(...) command substitution */
    TOK_IF,
    TOK_THEN,
    TOK_ELSE,
    TOK_FI,
    TOK_WHILE,
    TOK_DO,
    TOK_DONE,
    TOK_FOR,
    TOK_IN,
    TOK_FUNCTION,
    TOK_EOF,
    TOK_ERROR
} msh_token_type_t;

/* Token */
typedef struct {
    msh_token_type_t type;
    char            *value;
    int              line;
    int              col;
} msh_token_t;

/* Redirection types */
typedef enum {
    REDIR_NONE,
    REDIR_IN,       /* <  */
    REDIR_OUT,      /* >  */
    REDIR_APPEND,   /* >> */
    REDIR_ERR,      /* 2> */
    REDIR_ERR_APP,  /* 2>> */
    REDIR_BOTH,     /* &> */
    REDIR_BOTH_APP, /* &>> */
    REDIR_HEREDOC   /* << */
} msh_redir_type_t;

/* Redirection */
typedef struct msh_redir_s {
    msh_redir_type_t     type;
    char                *target;     /* filename or heredoc delimiter */
    int                  fd;         /* source fd */
    struct msh_redir_s  *next;
} msh_redir_t;

/* Simple command */
typedef struct {
    char       **argv;
    int          argc;
    msh_redir_t *redirs;
} msh_cmd_t;

/* Pipeline node */
typedef struct msh_pipeline_s {
    msh_cmd_t              *cmds;
    int                     ncmds;
    int                     background;
    struct msh_pipeline_s  *next;
} msh_pipeline_t;

/* AST root: list of pipelines */
typedef struct {
    msh_pipeline_t *head;
    int              background;  /* run in background */
    int              negated;     /* preceded by ! */
} msh_ast_t;

/* Job status */
typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} msh_job_status_t;

/* Job */
typedef struct msh_job_s {
    int                id;
    pid_t              pgid;
    char              *cmdline;
    msh_job_status_t   status;
    struct msh_job_s  *next;
    struct msh_job_s  *prev;
} msh_job_t;

/* History entry */
typedef struct {
    char *line;
    int   index;
} msh_hist_entry_t;

/* Alias */
typedef struct {
    char *name;
    char *value;
} msh_alias_t;

/* Shell state */
typedef struct {
    int          interactive;
    int          exit_code;
    int          last_status;
    char        *cwd;
    char        *home;
    char        *user;
    char        *host;
    char        *pos_params[10];  /* $1..$9 */
    msh_job_t   *jobs_head;
    int          next_job_id;
    msh_hist_entry_t *history;
    int          history_count;
    int          history_capacity;
    int          history_cursor;
    msh_alias_t  *aliases;
    int          alias_count;
    int          alias_capacity;
    char         prompt[MSH_MAX_PROMPT];
    int          debug;   /* debug flag */
    int          should_exit; /* set by exit builtin to request termination */
} msh_state_t;

/* Global state accessor */
msh_state_t *msh_get_state(void);
void         msh_state_init(void);
void         msh_state_cleanup(void);

/* Error reporting */
void msh_error(const char *fmt, ...);
void msh_perror(const char *prefix);

#endif /* MSH_H */