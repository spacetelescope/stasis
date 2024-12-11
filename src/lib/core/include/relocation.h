/**
 * @file relocation.h
 */
#ifndef STASIS_RELOCATION_H
#define STASIS_RELOCATION_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(STASIS_OS_DARWIN)
#include <limits.h>
# else
#include <linux/limits.h>
#endif
#include <unistd.h>

#define REPLACE_TRUNCATE_AFTER_MATCH 1

int replace_text(char *original, const char *target, const char *replacement, unsigned flags);
int file_replace_text(const char* filename, const char* target, const char* replacement, unsigned flags);

#endif //STASIS_RELOCATION_H
