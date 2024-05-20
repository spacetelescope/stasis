/**
 * @file relocation.h
 */
#ifndef OMC_RELOCATION_H
#define OMC_RELOCATION_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(OMC_OS_DARWIN)
#include <limits.h>
#elif defined(OMC_OS_LINUX)
#include <linux/limits.h>
#endif
#include <unistd.h>

#define REPLACE_TRUNCATE_AFTER_MATCH 1

int replace_text(char *original, const char *target, const char *replacement, unsigned flags);
int file_replace_text(const char* filename, const char* target, const char* replacement, unsigned flags);

#endif //OMC_RELOCATION_H
