//! @file core.h
#ifndef STASIS_CORE_H
#define STASIS_CORE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <sys/statvfs.h>

#define STASIS_BUFSIZ 8192
#define STASIS_NAME_MAX 255
#define STASIS_DIRSTACK_MAX 1024
#define STASIS_TIME_STR_MAX 128
#define HTTP_ERROR(X) X >= 400

#include "config.h"
#include "core_mem.h"
#include "core_message.h"

#define COE_CHECK_ABORT(COND, MSG) \
    do {\
        if (!globals.continue_on_error && (COND)) { \
            msg(STASIS_MSG_ERROR, MSG ": Aborting execution (--continue-on-error/-C is not enabled)\n"); \
            exit(1);                       \
        } \
    } while (0)

struct STASIS_GLOBAL {
    bool verbose; //!< Enable verbose output
    bool always_update_base_environment; //!< Update base environment immediately after activation
    bool continue_on_error; //!< Do not stop on test failures
    bool conda_fresh_start; //!< Always install a new copy of Conda
    bool enable_docker; //!< Enable docker image builds
    bool enable_artifactory; //!< Enable artifactory uploads
    bool enable_artifactory_build_info; //!< Enable build info (best disabled for pure test runs)
    bool enable_artifactory_upload; //!< Enable artifactory file upload (dry-run when false)
    bool enable_testing; //!< Enable package testing
    bool enable_overwrite; //!< Enable release file clobbering
    bool enable_rewrite_spec_stage_2; //!< Enable automatic @STR@ replacement in output files
    bool enable_parallel; //!< Enable testing in parallel
    bool enable_export; //!< Enable environment creation
    long cpu_limit; //!< Limit parallel processing to n cores (default: max - 1)
    long parallel_fail_fast; //!< Fail immediately on error
    int pool_status_interval; //!< Report "Task is running" every n seconds
    struct StrList *conda_packages; //!< Conda packages to install after initial activation
    struct StrList *pip_packages; //!< Pip packages to install after initial activation
    char *tmpdir; //!< Path to temporary storage directory
    char *conda_install_prefix; //!< Path to install conda
    char *sysconfdir; //!< Path where STASIS reads its configuration files (mission directory, etc)
    struct {
        char *tox_posargs;
        char *conda_reactivate;
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
        char *url;
    } jfrog;
    struct EnvCtl *envctl;
};
extern struct STASIS_GLOBAL globals;

extern const char *VERSION;
extern const char *AUTHOR;
extern const char *BANNER;

/**
 * Free memory allocated in global configuration structure
 */
void globals_free();

#endif //STASIS_CORE_H
