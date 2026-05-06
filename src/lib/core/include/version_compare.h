#ifndef STASIS_VERSION_COMPARE_H
#define STASIS_VERSION_COMPARE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "str.h"

#define GT 1 << 1
#define LT 1 << 2
#define EQ 1 << 3
#define NOT 1 << 4
#define EPOCH_MOD 10000

int version_sum(const char *str);
int version_parse_operator(const char *str);
int version_compare(int flags, const char *aa, const char *bb);

#endif //STASIS_VERSION_COMPARE_H