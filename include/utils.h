#ifndef OMC_UTILS_H
#define OMC_UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include "system.h"

#if defined(OMC_OS_WINDOWS)
#define PATH_ENV_VAR "path"
#define DIR_SEP "\\"
#define PATH_SEP ";"
#define LINE_SEP "\r\n"
#else
#define PATH_ENV_VAR "PATH"
#define DIR_SEP "/"
#define PATH_SEP ":"
#define LINE_SEP "\n"
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

#define OMC_MSG_SUCCESS 0
#define OMC_MSG_NOP 1 << 0
#define OMC_MSG_ERROR 1 << 1
#define OMC_MSG_WARN 1 << 2
#define OMC_MSG_L1 1 << 3
#define OMC_MSG_L2 1 << 4
#define OMC_MSG_L3 1 << 5
#define OMC_MSG_RESTRICT 1 << 6     ///< Restrict to verbose mode

void msg(unsigned type, char *fmt, ...);
void debug_shell();
/**
 * Creates a temporary file returning an open file pointer via @a fp, and the
 * path to the file. The caller is responsible for closing @a fp and
 * free()ing the returned file path.
 * @param fp pointer to FILE (to be initialized)
 * @return system path to the temporary file
 * @return NULL on failure
 */
char *xmkstemp(FILE **fp);
int isempty_dir(const char *path);

#endif //OMC_UTILS_H
