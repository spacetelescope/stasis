//! @file copy.h
#ifndef STASIS_COPY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include "core.h"

#define CT_OWNER 1 << 1
#define CT_PERM 1 << 2

/**
 * Recursively copy a directory, or a single file
 *
 * ```c
 * if (copytree("/source/path", "/destination/path", CT_PERM | CT_OWNER)) {
 *     fprintf(stderr, "Unable to copy files\n");
 *     exit(1);
 * }
 * ```
 *
 * @param srcdir source file or directory path
 * @param destdir destination directory
 * @param op CT_OWNER (preserve ownership)
 * @param op CT_PERM (preserve permission bits)
 * @return 0 on success, -1 on error
 */
int copytree(const char *srcdir, const char *destdir, unsigned op);

/**
 * Copy a single file
 *
 * ```c
 * if (copy2("/source/path/example.txt", "/destination/path/example.txt", CT_PERM | CT_OWNER)) {
 *     fprintf(stderr, "Unable to copy file\n");
 *     exit(1);
 * }
 * ```
 *
 *
 * @param src source file path
 * @param dest destination file path
 * @param op CT_OWNER (preserve ownership)
 * @param op CT_PERM (preserve permission bits)
 * @return 0 on success, -1 on error
 */
int copy2(const char *src, const char *dest, unsigned op);

#endif // STASIS_COPY_H