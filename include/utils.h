#ifndef OHMYCAL_UTILS_H
#define OHMYCAL_UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include "system.h"

#if defined(__WIN32__)
#define PATH_ENV_VAR "path"
#define DIR_SEP "\\"
#define PATH_SEP ";"
#else
#define PATH_ENV_VAR "PATH"
#define DIR_SEP "/"
#define PATH_SEP ":"
#endif

typedef int (ReaderFn)(size_t line, char **);

int pushd(const char *path);
int popd(void);
char *expandpath(const char *_path);
int rmtree(char *_path);
char **file_readlines(const char *filename, size_t start, size_t limit, ReaderFn *readerFn);
char *path_basename(char *path);
char *find_program(const char *name);
int touch(const char *filename);
int git_clone(struct Process *proc, char *url, char *destdir, char *gitref);
char *git_describe(const char *path);

#define OMC_MSG_NOP 1 << 0
#define OMC_MSG_ERROR 1 << 1
#define OMC_MSG_WARN 1 << 2
#define OMC_MSG_L1 1 << 3
#define OMC_MSG_L2 1 << 4
#define OMC_MSG_L3 1 << 5
int msg(unsigned type, char *fmt, ...);

#endif //OHMYCAL_UTILS_H
