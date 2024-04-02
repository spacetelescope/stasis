//! @file omc.h
#ifndef OMC_OMC_H
#define OMC_OMC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <sys/statvfs.h>

#define SYSERROR(MSG, ...) do { \
    fprintf(stderr, "%s:%s:%d:%s - ", path_basename(__FILE__), __FUNCTION__, __LINE__, strerror(errno) ? "info" : strerror(errno)); \
    fprintf(stderr, MSG LINE_SEP, __VA_ARGS__); \
} while (0)
#define OMC_BUFSIZ 8192
#define OMC_NAME_MAX 255
#define OMC_DIRSTACK_MAX 1024
#define OMC_TIME_STR_MAX 128
#define HTTP_ERROR(X) X >= 400

#include "config.h"
#include "template.h"

#include "utils.h"
#include "copy.h"
#include "ini.h"
#include "conda.h"
#include "environment.h"
#include "artifactory.h"
#include "docker.h"
#include "deliverable.h"
#include "str.h"
#include "strlist.h"
#include "system.h"
#include "download.h"
#include "recipe.h"
#include "relocation.h"
#include "wheel.h"

#define guard_runtime_free(X) do { if (X) { runtime_free(X); X = NULL; } } while (0)
#define guard_strlist_free(X) do { if ((*X)) { strlist_free(X); (*X) = NULL; } } while (0)
#define guard_free(X) do { if (X) { free(X); X = NULL; } } while (0)
#define GENERIC_ARRAY_FREE(ARR) do { \
    for (size_t ARR_I = 0; ARR && ARR[ARR_I] != NULL; ARR_I++) { \
        guard_free(ARR[ARR_I]); \
    } \
    guard_free(ARR); \
} while (0)

#define COE_CHECK_ABORT(COND, MSG) \
    do {\
        if (!globals.continue_on_error && COND) { \
            msg(OMC_MSG_ERROR, MSG ": Aborting execution (--continue-on-error/-C is not enabled)\n"); \
            exit(1);                       \
        } \
    } while (0)

struct OMC_GLOBAL {
    bool verbose; //!< Enable verbose output
    bool always_update_base_environment; //!< Update base environment immediately after activation
    bool continue_on_error; //!< Do not stop on test failures
    bool conda_fresh_start; //!< Always install a new copy of Conda
    bool enable_docker; //!< Enable docker image builds
    bool enable_artifactory; //!< Enable artifactory uploads
    bool enable_testing; //!< Enable package testing
    struct StrList *conda_packages; //!< Conda packages to install after initial activation
    struct StrList *pip_packages; //!< Pip packages to install after initial activation
    char *tmpdir; //!< Path to temporary storage directory
    char *conda_install_prefix; //!< Path to install conda
    char *sysconfdir; //!< Path where OMC reads its configuration files (mission directory, etc)
    struct {
        char *tox_posargs;
    } workaround;
    struct Jfrog {
        char *jfrog_artifactory_base_url;
        char *jfrog_artifactory_product;
        char *cli_major_ver;
        char *version;
        char *os;
        char *arch;
        char *remote_filename;
        char *repo;
    } jfrog;
};

/**
 * Free memory allocated in global configuration structure
 */
void globals_free();

#endif //OMC_OMC_H
