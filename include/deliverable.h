//
// Created by jhunk on 10/5/23.
//

#ifndef OHMYCAL_DELIVERABLE_H
#define OHMYCAL_DELIVERABLE_H

#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/utsname.h>
#include "str.h"
#include "ini.h"
#include "environment.h"

#define DELIVERY_DIR "delivery"
#define DELIVERY_PLATFORM_MAX 4
#define DELIVERY_PLATFORM_MAXLEN 65
#define DELIVERY_PLATFORM 0
#define DELIVERY_PLATFORM_CONDA_SUBDIR 1
#define DELIVERY_PLATFORM_CONDA_INSTALLER 2
#define DELIVERY_PLATFORM_RELEASE 3

#define INSTALL_PKG_CONDA 1 << 1
#define INSTALL_PKG_CONDA_DEFERRED 1 << 2
#define INSTALL_PKG_PIP 1 << 3
#define INSTALL_PKG_PIP_DEFERRED 1 << 4

struct Delivery {
    struct System {
        char *arch;
        char platform[DELIVERY_PLATFORM_MAX][DELIVERY_PLATFORM_MAXLEN];
    } system;
    struct Storage {
        char *delivery_dir;
        char *conda_install_prefix;
        char *conda_artifact_dir;
        char *conda_staging_dir;
        char *conda_staging_url;
        char *wheel_artifact_dir;
        char *wheel_staging_dir;
        char *wheel_staging_url;
        char *build_dir;
        char *build_recipes_dir;
        char *build_sources_dir;
        char *build_testing_dir;
    } storage;
    struct Meta {
        // delivery name
        char *name;
        // delivery version
        char *version;
        // build iteration
        int rc;
        // version of python to use
        char *python;
        // shortened python identifier
        char *python_compact;
        // URL to previous final configuration
        char *based_on;
        // hst, jwst, roman
        char *mission;
        // HST uses codenames
        char *codename;
        // is this a final release?
        bool final;
        // keep going, or don't
        bool continue_on_error;
    } meta;

    struct Info {
        // The fully combined release string
        char *release_name;
        struct tm *time_info;
        time_t time_now;
    } info;

    struct Conda {
        char *installer_baseurl;
        char *installer_name;
        char *installer_version;
        char *installer_platform;
        char *installer_arch;
        char *tool_version;
        char *tool_build_version;
        // packages to install
        struct StrList *conda_packages;
        // conda recipes to be built
        struct StrList *conda_packages_defer;
        // packages to install
        struct StrList *pip_packages;
        // packages to be built
        struct StrList *pip_packages_defer;
    } conda;

    // global runtime variables
    struct Runtime {
        RuntimeEnv *environ;
    } runtime;

    struct Test {
        char *name;
        char *version;
        char *repository;
        char *script;
        char *build_recipe;
        // test-specific runtime variables
        struct Runtime runtime;
    } tests[1000];
};

int delivery_init(struct Delivery *ctx, struct INIFILE *ini, struct INIFILE *cfg);
void delivery_free(struct Delivery *ctx);
void delivery_meta_show(struct Delivery *ctx);
void delivery_conda_show(struct Delivery *ctx);
void delivery_tests_show(struct Delivery *ctx);
int delivery_build_recipes(struct Delivery *ctx);
struct StrList *delivery_build_wheels(struct Delivery *ctx);
int delivery_index_wheel_artifacts(struct Delivery *ctx);
char *delivery_get_spec_header(struct Delivery *ctx);
void delivery_rewrite_spec(struct Delivery *ctx, char *filename);
int delivery_copy_wheel_artifacts(struct Delivery *ctx);
int delivery_copy_conda_artifacts(struct Delivery *ctx);
void delivery_get_installer(char *installer_url);
void delivery_get_installer_url(struct Delivery *delivery, char *result);
void delivery_install_packages(struct Delivery *ctx, char *conda_install_dir, char *env_name, int type, struct StrList *manifest[]);
int delivery_index_conda_artifacts(struct Delivery *ctx);
void delivery_tests_run(struct Delivery *ctx);

#endif //OHMYCAL_DELIVERABLE_H
