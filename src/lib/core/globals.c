#include <stdlib.h>
#include <stdbool.h>
#include "core.h"
#include "envctl.h"

const char *VERSION = "1.0.0";
const char *AUTHOR = "Joseph Hunkeler";
const char *BANNER =
        "------------------------------------------------------------------------\n"
#if defined(STASIS_DUMB_TERMINAL)
        "                                 STASIS                                 \n"
#else
        "                 _____ _______        _____ _____  _____                \n"
        "                / ____|__   __|/\\    / ____|_   _|/ ____|              \n"
        "               | (___    | |  /  \\  | (___   | | | (___                \n"
        "                \\___ \\   | | / /\\ \\  \\___ \\  | |  \\___ \\        \n"
        "                ____) |  | |/ ____ \\ ____) |_| |_ ____) |              \n"
        "               |_____/   |_/_/    \\_\\_____/|_____|_____/              \n"
        "\n"
#endif
        "------------------------------------------------------------------------\n"
        "                           Delivery Generator                           \n"
        "                                 v%s                                    \n"
        "------------------------------------------------------------------------\n"
        "Copyright (C) 2023-2024 %s,\n"
        "Association of Universities for Research in Astronomy (AURA)\n";

struct STASIS_GLOBAL globals = {
        .verbose = false, ///< Toggle verbose mode
        .continue_on_error = false, ///< Do not stop program on error
        .always_update_base_environment = false, ///< Run "conda update --all" after installing Conda
        .conda_fresh_start = true, ///< Remove/reinstall Conda at startup
        .conda_install_prefix = NULL, ///< Path to install Conda
        .conda_packages = NULL, ///< Conda packages to install
        .pip_packages = NULL, ///< Python packages to install
        .tmpdir = NULL, ///< Path to store temporary data
        .enable_docker = true, ///< Toggle docker usage
        .enable_artifactory = true, ///< Toggle artifactory server usage
        .enable_artifactory_build_info = true, ///< Toggle build-info uploads
        .enable_artifactory_upload = true, ///< Toggle artifactory file uploads
        .enable_export = true, //< Toggle conda environment creation and export
        .enable_testing = true, ///< Toggle [test] block "script" execution. "script_setup" always executes.
        .enable_rewrite_spec_stage_2 = true, ///< Leave template stings in output files
        .enable_parallel = true, ///< Toggle testing in parallel
        .parallel_fail_fast = false, ///< Kill ALL multiprocessing tasks immediately on error
        .pool_status_interval = 30, ///< Report "Task is running"
};

void globals_free() {
    guard_free(globals.tmpdir);
    guard_free(globals.sysconfdir);
    guard_free(globals.conda_install_prefix);
    guard_strlist_free(&globals.conda_packages);
    guard_strlist_free(&globals.pip_packages);
    guard_free(globals.jfrog.arch);
    guard_free(globals.jfrog.os);
    guard_free(globals.jfrog.url);
    guard_free(globals.jfrog.repo);
    guard_free(globals.jfrog.version);
    guard_free(globals.jfrog.cli_major_ver);
    guard_free(globals.jfrog.jfrog_artifactory_base_url);
    guard_free(globals.jfrog.jfrog_artifactory_product);
    guard_free(globals.jfrog.remote_filename);
    guard_free(globals.workaround.conda_reactivate);
    if (globals.envctl) {
        envctl_free(&globals.envctl);
    }
}
