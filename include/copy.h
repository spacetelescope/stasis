#ifndef OMC_COPY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include "omc.h"

#define CT_OWNER 1 << 1
#define CT_PERM 1 << 2

int copytree(const char *srcdir, const char *destdir, unsigned op);
int mkdirs(const char *_path, mode_t mode);
int copy2(const char *src, const char *dest, unsigned op);

#endif // OMC_COPY_H