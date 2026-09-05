#ifndef MSH_UTIL_H
#define MSH_UTIL_H

#include "msh.h"

void *msh_xmalloc(size_t size);
void *msh_xcalloc(size_t nmemb, size_t size);
void *msh_xrealloc(void *ptr, size_t size);
char *msh_strdup(const char *s);
char *msh_strndup(const char *s, size_t n);
char *msh_strconcat(const char *a, const char *b);
int   msh_strcmp(const char *a, const char *b);
int   msh_streq(const char *a, const char *b);
int   msh_strstarts(const char *s, const char *prefix);
int   msh_strends(const char *s, const char *suffix);
char *msh_strtrim(char *s);
char *msh_strip_quotes(const char *s);
int   msh_isspace(int c);
int   msh_isalnum(int c);
int   msh_isdigit(int c);

char **msh_strsplit(const char *s, const char *delim, int *count);
char **msh_strsplit_keep(const char *s, const char *delim, int *count);
void   msh_strfreev(char **argv);

int    msh_is_dir(const char *path);
int    msh_is_executable(const char *path);
char  *msh_abspath(const char *path);
char  *msh_path_join(const char *dir, const char *name);
char  *msh_basename(const char *path);
char  *msh_dirname(const char *path);
int    msh_file_exists(const char *path);
ssize_t msh_read_line(int fd, char *buf, size_t max);
ssize_t msh_write_all(int fd, const char *buf, size_t len);

void  *msh_sbrk_alloc(size_t size);  /* fallback allocator */

#endif