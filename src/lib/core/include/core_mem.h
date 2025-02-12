//! @file core_mem.h
#ifndef STASIS_CORE_MEM_H
#define STASIS_CORE_MEM_H

#include "environment.h"
#include "strlist.h"

#define guard_runtime_free(X) do { if (X) { runtime_free(X); (X) = NULL; } } while (0)
#define guard_strlist_free(X) do { if ((*X)) { strlist_free(X); (*X) = NULL; } } while (0)
#define guard_free(X) do { if (X) { free(X); X = NULL; } } while (0)
#define GENERIC_ARRAY_FREE(ARR) do { \
    for (size_t ARR_I = 0; ARR && ARR[ARR_I] != NULL; ARR_I++) { \
        guard_free(ARR[ARR_I]); \
    } \
    guard_free(ARR); \
} while (0)

#endif //STASIS_CORE_MEM_H
