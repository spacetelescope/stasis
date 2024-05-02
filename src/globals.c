#include <stdlib.h>
#include <stdbool.h>
#include "omc.h"

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
                     "                          Delivery Generator                         \n"
                     "                                v%s                                  \n"
                     "---------------------------------------------------------------------\n"
                     "Copyright (C) 2023-2024 %s,\n"
                     "Association of Universities for Research in Astronomy (AURA)\n";

struct OMC_GLOBAL globals = {
        .verbose = false,
        .continue_on_error = false,
        .always_update_base_environment = false,
        .conda_fresh_start = true,
        .conda_install_prefix = NULL,
        .conda_packages = NULL,
        .pip_packages = NULL,
        .tmpdir = NULL,
        .enable_docker = true,
        .enable_artifactory = true,
        .enable_testing = true,
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
    guard_free(globals.workaround.tox_posargs);
}
