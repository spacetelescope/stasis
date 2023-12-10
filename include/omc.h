#ifndef OMC_OMC_H
#define OMC_OMC_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#define SYSERROR stderr, "%s:%s:%d: %s\n", path_basename(__FILE__), __FUNCTION__, __LINE__, strerror(errno)
#define OMC_BUFSIZ 8192

#include "utils.h"
#include "ini.h"
#include "conda.h"
#include "environment.h"
#include "deliverable.h"
#include "str.h"
#include "strlist.h"
#include "system.h"
#include "download.h"
#include "recipe.h"
#include "relocation.h"

#define guard_runtime_free(X) if (X) { runtime_free(X); X = NULL; }
#define guard_strlist_free(X) if (X) { strlist_free(X); X = NULL; }
#define guard_free(X) if (X) { free(X); X = NULL; }

#define COE_CHECK_ABORT(COND, MSG) {\
    if (COND) { \
        msg(OMC_MSG_ERROR, MSG ": Aborting execution (--continue-on-error/-C is not enabled)\n"); \
        exit(1); \
    } \
}

struct OMC_GLOBAL {
    unsigned char verbose;
    unsigned char always_update_base_environment;
    unsigned char continue_on_error;
    struct StrList *conda_packages;
    struct StrList *pip_packages;
    char *tmpdir;
};

#endif //OMC_OMC_H
