//! @file utils.h
#ifndef STASIS_UTILS_H
#define STASIS_UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include "core.h"
#include "copy.h"
#include "system.h"
#include "strlist.h"
#include "utils.h"
#include "ini.h"

#if defined(STASIS_OS_WINDOWS)
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

#define STASIS_XML_PRETTY_PRINT_PROG "xmllint"
#define STASIS_XML_PRETTY_PRINT_ARGS "--format"

/**
 * Change directory. Push path on directory stack.
 *
 * ```c
 * pushd("/somepath");
 *
 * FILE fp = fopen("somefile", "w"); // i.e. /somepath/somefile
 * fprintf(fp, "Hello world.\n");
 * fclose(fp);
 *
 * popd();
 * ```
 *
 * @param path of directory
 * @return 0 on success, -1 on error
 */
int pushd(const char *path);

/**
 * Return from directory. Pop last path from directory stack.
 *
 * @see pushd
 * @return 0 on success, -1 if stack is empty
 */
int popd(void);

/**
 * Expand "~" to the user's home directory
 *
 * ```c
 * char *home = expandpath("~");             // == /home/username
 * char *config = expandpath("~/.config");   // == /home/username/.config
 * char *nope = expandpath("/tmp/test");     // == /tmp/test
 * char *nada = expandpath("/~/broken");     // == /~/broken
 *
 * free(home);
 * free(config);
 * free(nope);
 * free(nada);
 * ```
 *
 * @param _path (Must start with a `~`)
 * @return success=expanded path or original path, failure=NULL
 */
char *expandpath(const char *_path);

/**
 * Remove a directory tree recursively
 *
 * ```c
 * mkdirs("a/b/c");
 * rmtree("a");
 * // a/b/c is removed
 * ```
 *
 * @param _path
 * @return 0 on success, -1 on error
 */
int rmtree(char *_path);


char **file_readlines(const char *filename, size_t start, size_t limit, ReaderFn *readerFn);

/**
 * Strip directory from file name
 * Note: Caller is responsible for freeing memory
 *
 * @param _path
 * @return success=file name, failure=NULL
 */
char *path_basename(char *path);

/**
 * Return parent directory of file, or the parent of a directory
 *
 * @param path
 * @return success=directory, failure=empty string
 */
char *path_dirname(char *path);

/**
 * Scan PATH directories for a named program
 * @param name program name
 * @return path to program, or NULL on error
 */
char *find_program(const char *name);

/**
 * Create an empty file, or update modified timestamp on an existing file
 * @param filename file to touch
 * @return 0 on success, 1 on error
 */
int touch(const char *filename);

/**
 * Clone a git repository
 *
 * ```c
 * struct Process proc;
 * memset(proc, 0, sizeof(proc));
 *
 * if (git_clone(&proc, "https://github.com/myuser/myrepo", "./repos", "unstable_branch")) {
 *     fprintf(stderr, "Failed to clone repository\n");
 *     exit(1);
 * }
 *
 * if (pushd("./repos/myrepo")) {
 *     fprintf(stderr, "Unable to enter repository directory\n");
 * } else {
 *     // do something with repository
 *     popd();
 * }
 * ```
 *
 * @see pushd
 *
 * @param proc Process struct
 * @param url URL (or file system path) of repoistory to clone
 * @param destdir destination directory
 * @param gitref commit/branch/tag of checkout (NULL will use HEAD of default branch for repo)
 * @return exit code from "git"
 */
int git_clone(struct Process *proc, char *url, char *destdir, char *gitref);

/**
 * Git describe wrapper
 * @param path to repository
 * @return output from "git describe", or NULL on error
 */
char *git_describe(const char *path);

/**
 * Git rev-parse wrapper
 * @param path to repository
 * @param args to pass to git rev-parse
 * @return output from "git rev-parse", or NULL on error
 */
char *git_rev_parse(const char *path, char *args);

/**
 * Helper function to initialize simple STASIS internal path strings
 *
 * ```c
 * char *mypath = NULL;
 *
 * if (path_store(&mypath, PATH_MAX, "/some", "path")) {
 *     fprintf(stderr, "Unable to allocate memory for path elements\n");
 *     exit(1);
 * }
 * // mypath is allocated to size PATH_MAX and contains the string: /some/path
 * // base+path will truncate at maxlen - 1
 * ```
 *
 * @param destptr address of destination string pointer
 * @param maxlen maximum length of the path
 * @param base path
 * @param path to append to base
 * @return 0 on success, -1 on error
 */
int path_store(char **destptr, size_t maxlen, const char *base, const char *path);

#if defined(STASIS_DUMB_TERMINAL)
#define STASIS_COLOR_RED ""
#define STASIS_COLOR_GREEN ""
#define STASIS_COLOR_YELLOW ""
#define STASIS_COLOR_BLUE ""
#define STASIS_COLOR_WHITE ""
#define STASIS_COLOR_RESET ""
#else
//! Set output color to red
#define STASIS_COLOR_RED "\e[1;91m"
//! Set output color to green
#define STASIS_COLOR_GREEN "\e[1;92m"
//! Set output color to yellow
#define STASIS_COLOR_YELLOW "\e[1;93m"
//! Set output color to blue
#define STASIS_COLOR_BLUE "\e[1;94m"
//! Set output color to white
#define STASIS_COLOR_WHITE "\e[1;97m"
//! Reset output color to terminal default
#define STASIS_COLOR_RESET "\e[0;37m\e[0m"
#endif

#define STASIS_MSG_SUCCESS 0
//! Suppress printing of the message text
#define STASIS_MSG_NOP 1 << 0
//! The message is an error
#define STASIS_MSG_ERROR 1 << 1
//! The message is a warning
#define STASIS_MSG_WARN 1 << 2
//! The message will be indented once
#define STASIS_MSG_L1 1 << 3
//! The message will be indented twice
#define STASIS_MSG_L2 1 << 4
//! The message will be indented thrice
#define STASIS_MSG_L3 1 << 5
//! The message will only be printed in verbose mode
#define STASIS_MSG_RESTRICT 1 << 6

void msg(unsigned type, char *fmt, ...);

// Enter an interactive shell that ends the program on-exit
void debug_shell();

/**
 * Creates a temporary file returning an open file pointer via @a fp, and the
 * path to the file. The caller is responsible for closing @a fp and
 * free()ing the returned file path.
 *
 * ```c
 * FILE *fp = NULL;
 * char *tempfile = xmkstemp(&fp, "r+");
 * if (!fp || !tempfile) {
 *     fprintf(stderr, "Failed to generate temporary file for read/write\n");
 *     exit(1);
 * }
 * ```
 *
 * @param fp pointer to FILE (to be initialized)
 * @param mode fopen() style file mode string
 * @return system path to the temporary file
 * @return NULL on failure
 */
char *xmkstemp(FILE **fp, const char *mode);

/**
 * Is the path an empty directory structure?
 *
 * ```c
 * if (isempty_dir("/some/path")) {
 *     fprintf(stderr, "The directory is is empty!\n");
 * } else {
 *     printf("The directory contains dirs/files\n");
 * }
 * ```
 *
 * @param path directory
 * @return 0 = no, 1 = yes
 */
int isempty_dir(const char *path);

/**
 * Rewrite an XML file with a pretty printer command
 * @param filename path to modify
 * @param pretty_print_prog program to call
 * @param pretty_print_args arguments to pass to program
 * @return 0 on success, -1 on error
 */
int xml_pretty_print_in_place(const char *filename, const char *pretty_print_prog, const char *pretty_print_args);

/**
 * Applies STASIS fixups to a tox ini config
 * @param filename path to tox.ini
 * @param result path to processed configuration
 * @return 0 on success, -1 on error
 */
int fix_tox_conf(const char *filename, char **result);

char *collapse_whitespace(char **s);

/**
 * Write ***REDACTED*** in dest for each occurrence of to_redacted token present in src
 *
 * ```c
 * char command[PATH_MAX] = {0};
 * char command_redacted[PATH_MAX] = {0};
 * const char *password = "abc123";
 * const char *host = "myhostname";
 * const char *to_redact_case1[] = {password, host, NULL};
 * const char *to_redact_case2[] = {password, "--host", NULL};
 * const char *to_redact_case3[] = {password, "--host", host, NULL};
 *
 * sprintf(command, "echo %s | program --host=%s -", password, host);
 *
 * // CASE 1
 * redact_sensitive(to_redact_case1, command, command_redacted, sizeof(command_redacted) - 1);
 * printf("executing: %s\n", command_redacted);
 * // User sees:
 * // executing: echo ***REDACTED*** | program --host=***REDACTED*** -
 * system(command);
 *
 * // CASE 2 remove an entire argument
 * redact_sensitive(to_redact_case2, command, command_redacted, sizeof(command_redacted) - 1);
 * printf("executing: %s\n", command_redacted);
 * // User sees:
 * // executing: echo ***REDACTED*** | program ***REDACTED*** -
 * system(command);
 *
 * // CASE 3 remove it all (noisy)
 * redact_sensitive(to_redact_case3, command, command_redacted, sizeof(command_redacted) - 1);
 * printf("executing: %s\n", command_redacted);
 * // User sees:
 * // executing: echo ***REDACTED*** | program ***REDACTED***=***REDACTED*** -
 * system(command);
 * ```
 *
 * @param to_redact array of tokens to redact
 * @param src input string
 * @param dest output string
 * @param maxlen maximum length of dest byte array
 * @return 0 on success, -1 on error
 */
int redact_sensitive(const char **to_redact, size_t to_redact_size, char *src, char *dest, size_t maxlen);

/**
 * Given a directory path, return a list of files
 *
 * ~~~{.c}
 * struct StrList *files;
 *
 * basepath = ".";
 * files = listdir(basepath);
 * for (size_t i = 0; i < strlist_count(files); i++) {
 *     char *filename = strlist_item(files, i);
 *     printf("%s/%s\n", basepath, filename);
 * }
 * guard_strlist_free(&files);
 * ~~~
 *
 * @param path of a directory
 * @return a StrList containing file names
 */
struct StrList *listdir(const char *path);

/**
 * Get CPU count
 * @return CPU count on success, zero on error
 */
long get_cpu_count();

/**
 * Create all leafs in directory path
 * @param _path directory path to create
 * @param mode mode_t permissions
 * @return
 */
int mkdirs(const char *_path, mode_t mode);

/**
 * Return pointer to a (possible) version specifier
 *
 * ```c
 * char s[] = "abc==1.2.3";
 * char *spec_begin = find_version_spec(s);
 * // spec_begin is "==1.2.3"
 *
 * char package_name[255];
 * char s[] = "abc";
 * char *spec_pos = find_version_spec(s);
 * if (spec_pos) {
 *     strncpy(package_name, spec_pos - s);
 *     // use spec
 * } else {
 *     // spec not found
 * }
 *
 * @param str a pointer to a buffer containing a package spec (i.e. abc==1.2.3, abc>=1.2.3, abc)
 * @return a pointer to the first occurrence of a version spec character
 * @return NULL if not found
 */
char *find_version_spec(char *package_name);

// mode flags for env_manipulate_pathstr
#define PM_APPEND 1 << 0
#define PM_PREPEND 1 << 1
#define PM_ONCE  1 << 2

/**
* Add paths to the head or tail of an environment variable.
*
* @param key environment variable to manipulate
* @param path to insert (does not need to exist)
* @param mode PM_APPEND `$path:$PATH`
* @param mode PM_PREPEND `$PATH:path`
* @param mode PM_ONCE do not manipulate if `path` is present in PATH variable
*/
int env_manipulate_pathstr(const char *key, char *path, int mode);

/**
* Append or replace a file extension
*/
int gen_file_extension_str(char *filename, const char *extension);

#endif //STASIS_UTILS_H
