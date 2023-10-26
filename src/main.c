#define GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/utsname.h>
#include "ohmycal.h"
#include "wheel.h"

const char *VERSION = "1.0.0";
const char *AUTHOR = "Joseph Hunkeler";
const char *BANNER = "---------------------------------------------------------------------\n"
                     " ██████╗ ██╗  ██╗    ███╗   ███╗██╗   ██╗     ██████╗ █████╗ ██╗     \n"
                     "██╔═══██╗██║  ██║    ████╗ ████║╚██╗ ██╔╝    ██╔════╝██╔══██╗██║     \n"
                     "██║   ██║███████║    ██╔████╔██║ ╚████╔╝     ██║     ███████║██║     \n"
                     "██║   ██║██╔══██║    ██║╚██╔╝██║  ╚██╔╝      ██║     ██╔══██║██║     \n"
                     "╚██████╔╝██║  ██║    ██║ ╚═╝ ██║   ██║       ╚██████╗██║  ██║███████╗\n"
                     " ╚═════╝ ╚═╝  ╚═╝    ╚═╝     ╚═╝   ╚═╝        ╚═════╝╚═╝  ╚═╝╚══════╝\n"
                     "---------------------------------------------------------------------\n"
                     "                     Delivery Generator                              \n"
                     "                           v%s\n"
                     "---------------------------------------------------------------------\n"
                     "Copyright (C) 2023 %s,\n"
                     "Association of Universities for Research in Astronomy (AURA)\n";


void conda_setup_headless() {
    // Configure conda for headless CI
    conda_exec("config --system --set auto_update_conda false");
    conda_exec("config --system --set always_yes true");
    conda_exec("config --system --set quiet true");
    conda_exec("config --system --set rollback_enabled false");
    conda_exec("config --system --set report_errors false");

    if (conda_exec("update --all")) {
        perror("update base");
        exit(1);
    }
}

void delivery_install_conda(char *install_script, char *conda_install_dir) {
    struct Process proc;
    memset(&proc, 0, sizeof(proc));

    if (!access(conda_install_dir, F_OK)) {
        if (rmtree(conda_install_dir)) {
            perror("unable to remove previous installation");
            exit(1);
        }
    }

    // -b = batch mode
    if (shell_safe(&proc, (char *[]) {find_program("bash"), install_script, "-b", "-p", conda_install_dir, NULL})) {
        fprintf(stderr, "conda installation failed\n");
        exit(1);
    }
}

void delivery_conda_enable(struct Delivery *ctx, char *conda_install_dir) {
    if (conda_activate(conda_install_dir, "base")) {
        fprintf(stderr, "conda activation failed\n");
        exit(1);
    }

    if (runtime_replace(&ctx->runtime.environ, __environ)) {
        perror("unable to replace runtime environment after activating conda");
        exit(1);
    }

    conda_setup_headless();
}

#define DEFER_CONDA 0
#define DEFER_PIP 1
void delivery_defer_packages(struct Delivery *ctx, int type) {
    struct StrList *dataptr = NULL;
    struct StrList *deferred = NULL;
    char *name = NULL;
    char cmd[PATH_MAX];

    memset(cmd, 0, sizeof(cmd));

    char mode[10];
    if (DEFER_CONDA == type) {
        dataptr = ctx->conda.conda_packages;
        deferred = ctx->conda.conda_packages_defer;
        strcpy(mode, "conda");
    } else if (DEFER_PIP == type) {
        dataptr = ctx->conda.pip_packages;
        deferred = ctx->conda.pip_packages_defer;
        strcpy(mode, "pip");
    }
    msg(OMC_MSG_L2, "Filtering %s packages by test definition...\n", mode);

    struct StrList *filtered = NULL;
    filtered = strlist_init();
    for (size_t i = 0, z = 0; i < strlist_count(dataptr); i++) {
        name = strlist_item(dataptr, i);
        if (!strlen(name) || isblank(*name) || isspace(*name)) {
            continue;
        }
        msg(OMC_MSG_L3, "package '%s': ", name);
        int ignore_pkg = 0;
        for (size_t x = 0; x < sizeof(ctx->tests) / sizeof(ctx->tests[0]); x++) {
            if (ctx->tests[x].name) {
                if (startswith(ctx->tests[x].name, name)) {
                    ignore_pkg = 1;
                    z++;
                    break;
                }
            }
        }

        if (ignore_pkg) {
            printf("BUILD FOR HOST\n");
            strlist_append(deferred, name);
        } else {
            printf("USE EXISTING\n");
            strlist_append(filtered, name);
        }
    }

    if (!strlist_count(deferred)) {
        msg(OMC_MSG_WARN, "No packages were filtered by test definitions");
    } else {
        if (DEFER_CONDA == type) {
            strlist_free(ctx->conda.conda_packages);
            ctx->conda.conda_packages = strlist_copy(filtered);
        } else if (DEFER_PIP == type) {
            strlist_free(ctx->conda.pip_packages);
            ctx->conda.pip_packages = strlist_copy(filtered);
        }
    }
}

void testfunc(struct Delivery *ctx, char *env_name) {
    struct Wheel *wheel;
    wheel = get_wheel_file(ctx->storage.wheel_artifact_dir, "drizzlepac", (char *[]) {"cp310", "x86_64", NULL});
    return;
}

int main(int argc, char *argv[], char *arge[]) {
    struct INIFILE *cfg = NULL;
    struct INIFILE *ini = NULL;
    struct Delivery ctx;
    struct Process proc = {
            .stdout = "",
            .stderr = "",
            .redirect_stderr = 0,
    };
    struct tm *tm_info;
    time_t timenow;
    char env_date[100];
    char env_name[PATH_MAX];
    char env_name_testing[PATH_MAX];
    char env_pyver[10];
    char *delivery_input = argv[1];
    char *config_input = argv[2];
    char installer_url[PATH_MAX];

    memset(&proc, 0, sizeof(proc));
    memset(&ctx, 0, sizeof(ctx));

    if (!delivery_input) {
        fprintf(stderr, "Missing *.ini file\n");
        exit(1);
    }

    msg(OMC_MSG_L1, "Initializing\n");
    struct utsname uts;
    uname(&uts);

    msg(OMC_MSG_L2, "Setting architecture\n");
    char archsuffix[255];
    ctx.system.arch = strdup(uts.machine);
    if (!strcmp(ctx.system.arch, "x86_64")) {
        strcpy(archsuffix, "64");
    } else {
        strcpy(archsuffix, ctx.system.arch);
    }

    msg(OMC_MSG_L2, "Setting platform\n");
    strcpy(ctx.system.platform[DELIVERY_PLATFORM], uts.sysname);
    if (!strcmp(ctx.system.platform[DELIVERY_PLATFORM], "Darwin")) {
        sprintf(ctx.system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], "osx-%s", archsuffix);
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], "MacOSX");
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_RELEASE], "macos");
    } else if (!strcmp(ctx.system.platform[DELIVERY_PLATFORM], "Linux")) {
        sprintf(ctx.system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], "linux-%s", archsuffix);
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], "Linux");
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_RELEASE], "linux");
    } else {
        // Not explicitly supported systems
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], ctx.system.platform[DELIVERY_PLATFORM]);
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], ctx.system.platform[DELIVERY_PLATFORM]);
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_RELEASE], ctx.system.platform[DELIVERY_PLATFORM]);
        tolower_s(ctx.system.platform[DELIVERY_PLATFORM_RELEASE]);
    }

    msg(OMC_MSG_L2, "Setting up runtime environment...\n");
    setenv("OMC_ARCH", ctx.system.arch, 1);
    setenv("OMC_PLATFORM", ctx.system.platform[DELIVERY_PLATFORM], 1);
    setenv("OMC_CONDA_ARCH", ctx.system.arch, 1);
    setenv("OMC_CONDA_PLATFORM", ctx.system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], 1);
    setenv("OMC_CONDA_PLATFORM_SUBDIR", ctx.system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], 1);

    if (config_input) {
        msg(OMC_MSG_L2, "Reading OMC global configuration: %s\n", config_input);
        cfg = ini_open(config_input);
        //ini_show(cfg);
    }

    msg(OMC_MSG_L2, "Reading OMC delivery configuration: %s\n", delivery_input);
    ini = ini_open(delivery_input);
    //ini_show(ini);

    printf(BANNER, VERSION, AUTHOR);

    delivery_init(&ctx, ini, cfg);
    runtime_apply(ctx.runtime.environ);
    msg(OMC_MSG_L1, "Overview\n");
    delivery_meta_show(&ctx);
    delivery_conda_show(&ctx);
    delivery_tests_show(&ctx);

    msg(OMC_MSG_L1, "Conda setup\n");
    delivery_get_installer_url(&ctx, installer_url);
    msg(OMC_MSG_L2, "Downloading: %s\n", installer_url);
    delivery_get_installer(installer_url);

    // Unlikely to occur: this should help prevent rmtree() from destroying your entire filesystem
    // if path is "/" then, die
    // or if empty string, die
    if (!strcmp(ctx.storage.conda_install_prefix, DIR_SEP) || !strlen(ctx.storage.conda_install_prefix)) {
        fprintf(stderr, "error: ctx.storage.conda_install_prefix is malformed!\n");
        exit(1);
    }

    msg(OMC_MSG_L2, "Installing: %s\n", path_basename(installer_url));
    delivery_install_conda(path_basename(installer_url), ctx.storage.conda_install_prefix);

    msg(OMC_MSG_L2, "Configuring: %s\n", ctx.storage.conda_install_prefix);
    delivery_conda_enable(&ctx, ctx.storage.conda_install_prefix);

    // Generate release and/or environment name
    memset(env_pyver, 0, sizeof(env_pyver));
    char *tmp_pyver = to_short_version(ctx.meta.python);
    sprintf(env_pyver, "py%s", tmp_pyver);
    free(tmp_pyver);
    tmp_pyver = NULL;

    msg(OMC_MSG_L1, "Generating release string\n");
    if (!strcasecmp(ctx.meta.mission, "hst") && ctx.meta.final) {
        memset(env_date, 0, sizeof(env_date));
        strftime(env_date, sizeof(env_date) - 1, "%Y%m%d", tm_info);
        sprintf(env_name, "%s_%s_rc%d", ctx.meta.name, env_date, ctx.meta.rc);
    } else if (!strcasecmp(ctx.meta.mission, "hst")) {
        sprintf(env_name, "%s_%s_%s_%s_rc%d", ctx.meta.name, ctx.meta.codename, ctx.system.platform[DELIVERY_PLATFORM_RELEASE], env_pyver, ctx.meta.rc);
    } else if (!strcasecmp(ctx.meta.mission, "jwst") && ctx.meta.final) {
        sprintf(env_name, "%s_%s_rc%d", ctx.meta.name, ctx.meta.version, ctx.meta.rc);
    } else if (!strcasecmp(ctx.meta.mission, "jwst")) {
        sprintf(env_name, "%s_%s_final", ctx.meta.name, ctx.meta.version);
    }
    msg(OMC_MSG_L2, "%s\n", env_name);
    sprintf(env_name_testing, "%s_test", env_name);

    msg(OMC_MSG_L1, "Creating release environment(s)\n");
    if (ctx.meta.based_on && strlen(ctx.meta.based_on)) {
        conda_env_remove(env_name);
        conda_env_create_from_uri(env_name, ctx.meta.based_on);

        conda_env_remove(env_name_testing);
        conda_env_create_from_uri(env_name_testing, ctx.meta.based_on);
    } else {
        conda_env_create(env_name, ctx.meta.python, NULL);
        conda_env_create(env_name_testing, ctx.meta.python, NULL);
    }

    // Activate test environment
    msg(OMC_MSG_L1, "Activating test environment\n");
    if (conda_activate(ctx.storage.conda_install_prefix, env_name_testing)) {
        fprintf(stderr, "failed to activate test environment\n");
        exit(1);
    }

    msg(OMC_MSG_L2, "Installing build tools\n");
    if (conda_exec("install boa conda-build conda-verify")) {
        msg(OMC_MSG_ERROR | OMC_MSG_L2, "conda-build installation failed");
        exit(1);
    }

    if (pip_exec("install build")) {
        msg(OMC_MSG_ERROR | OMC_MSG_L2, "'build' tool installation failed");
        exit(1);
    }

    time(&timenow);
    tm_info = localtime(&timenow);

    // Execute configuration-defined tests
    msg(OMC_MSG_L1, "Begin test execution\n");
    delivery_tests_run(&ctx);

    msg(OMC_MSG_L1, "Generating deferred package listing\n");
    // Test succeeded so move on to producing package artifacts
    delivery_defer_packages(&ctx, DEFER_CONDA);
    delivery_defer_packages(&ctx, DEFER_PIP);

    // TODO: wheels would be nice, but can't right now
    //if (ctx.conda.pip_packages_defer) {
    //    if (!delivery_build_wheels(&ctx)) {
    //        exit(1);
    //    }
    //    if (delivery_copy_wheel_artifacts(&ctx)) {
    //        exit(1);
    //    }
    //    if (delivery_index_wheel_artifacts(&ctx)) {
    //        exit(1);
    //    }
    //}

    if (ctx.conda.conda_packages_defer) {
        msg(OMC_MSG_L2, "Building Conda recipe(s)\n");
        if (delivery_build_recipes(&ctx)) {
            exit(1);
        }
        msg(OMC_MSG_L3, "Copying artifacts\n");
        if (delivery_copy_conda_artifacts(&ctx)) {
            exit(1);
        }
        msg(OMC_MSG_L3, "Indexing artifacts\n");
        if (delivery_index_conda_artifacts(&ctx)) {
            exit(1);
        }
    }

    // Populate the release environment
    msg(OMC_MSG_L1, "Populating release environment\n");

    msg(OMC_MSG_L2, "Installing conda packages\n");
    delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_CONDA, (struct StrList *[]) {ctx.conda.conda_packages, NULL});
    msg(OMC_MSG_L3, "Installing deferred conda packages\n");
    delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_CONDA | INSTALL_PKG_CONDA_DEFERRED, (struct StrList *[]) {ctx.conda.conda_packages_defer, NULL});
    msg(OMC_MSG_L2, "Installing pip packages\n");
    delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_PIP, (struct StrList *[]) {ctx.conda.pip_packages, NULL});
    msg(OMC_MSG_L3, "Installing deferred pip packages\n");
    delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_PIP | INSTALL_PKG_PIP_DEFERRED, (struct StrList *[]) {ctx.conda.pip_packages_defer, NULL});

    conda_exec("list");

    msg(OMC_MSG_L1, "Creating release\n");
    msg(OMC_MSG_L2, "Exporting %s\n", env_name_testing);
    conda_env_export(env_name_testing, ctx.storage.delivery_dir, env_name_testing);

    msg(OMC_MSG_L2, "Exporting %s\n", env_name);
    conda_env_export(env_name, ctx.storage.delivery_dir, env_name);

    // Rewrite release environment output (i.e. set package origin(s) to point to the deployment server, etc.)
    char specfile[PATH_MAX];
    sprintf(specfile, "%s/%s.yml", ctx.storage.delivery_dir, env_name);
    msg(OMC_MSG_L3, "Rewriting release file %s\n", path_basename(specfile));
    delivery_rewrite_spec(&ctx, specfile);

    msg(OMC_MSG_L1, "Cleaning up\n");
    ini_free(&ini);
    ini_free(&cfg);

    msg(OMC_MSG_L1, "Done!\n");
    return 0;
}

