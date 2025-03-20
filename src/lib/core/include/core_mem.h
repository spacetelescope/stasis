//! @file core_mem.h
#ifndef STASIS_CORE_MEM_H
#define STASIS_CORE_MEM_H

#include "environment.h"
#include "strlist.h"

#define guard_runtime_free(X) do { runtime_free(X); (X) = NULL; } while (0)
#define guard_strlist_free(X) do { strlist_free(X); (*X) = NULL; } while (0)
#define guard_free(X) do { free(X); (X) = NULL; } while (0)
#define ARRAY_COUNT(ARR) sizeof((ARR)) / sizeof((*ARR))
#define guard_array_free_by_count(ARR, COUNT) do { \
    for (size_t ARR_I = 0; (ARR) && ARR_I < (COUNT); ARR_I++) { \
        guard_free((ARR)[ARR_I]); \
    } \
    guard_free((ARR)); \
} while (0)
#define guard_array_free(ARR) do { \
    for (size_t ARR_I = 0; ARR && ARR[ARR_I] != NULL; ARR_I++) { \
        guard_free(ARR[ARR_I]); \
    } \
    guard_free(ARR); \
} while (0)

#endif //STASIS_CORE_MEM_H
