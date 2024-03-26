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
# else
#include <linux/limits.h>
#endif
#include <unistd.h>

int replace_text(char *original, const char *target, const char *replacement);
int file_replace_text(const char* filename, const char* target, const char* replacement);

#endif //OMC_RELOCATION_H
