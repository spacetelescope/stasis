#include <stdlib.h>
#include <stdbool.h>
#include "core.h"

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
        .envctl = {
            {.flags = STASIS_ENVCTL_PASSTHRU, .name = {"TMPDIR", NULL}},
            {.flags = STASIS_ENVCTL_PASSTHRU, .name = {"STASIS_ROOT", NULL}},
            {.flags = STASIS_ENVCTL_PASSTHRU, .name = {"STASIS_SYSCONFDIR", NULL}},
            {.flags = STASIS_ENVCTL_PASSTHRU, .name = {"STASIS_CPU_COUNT", "CPU_COUNT", NULL}},
            {.flags = STASIS_ENVCTL_REQUIRED | STASIS_ENVCTL_REDACT, .name={"STASIS_GH_TOKEN", "GITHUB_TOKEN", NULL}},
            {.flags = STASIS_ENVCTL_REDACT, .name = {"STASIS_JF_ACCESS_TOKEN", NULL}},
            {.flags = STASIS_ENVCTL_PASSTHRU, .name = {"STASIS_JF_USER", NULL}},
            {.flags = STASIS_ENVCTL_REDACT, .name = {"STASIS_JF_PASSWORD", NULL}},
            {.flags = STASIS_ENVCTL_REDACT, .name = {"STASIS_JF_SSH_KEY_PATH", NULL}},
            {.flags = STASIS_ENVCTL_REDACT, .name = {"STASIS_JF_SSH_PASSPHRASE", NULL}},
            {.flags = STASIS_ENVCTL_REDACT, .name = {"STASIS_JF_CLIENT_CERT_CERT_PATH", NULL}},
            {.flags = STASIS_ENVCTL_REDACT, .name = {"STASIS_JF_CLIENT_CERT_KEY_PATH", NULL}},
            {.flags = STASIS_ENVCTL_REQUIRED, .name = {"STASIS_JF_REPO", NULL}},
            {.flags = 0, .name = {NULL}},
        }
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
    guard_free(globals.workaround.conda_reactivate);
}
