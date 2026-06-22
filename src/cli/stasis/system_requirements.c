#include "system_requirements.h"

void check_system_env_requirements() {
    msg(STASIS_MSG_L1, "Checking environment\n");
    globals.envctl = envctl_init();
    if (!globals.envctl) {
        SYSERROR("envctl_init failed");
        exit(1);
    }

    int status = 0;
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_PASSTHRU, NULL, "TMPDIR");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_PASSTHRU, NULL, "STASIS_ROOT");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_PASSTHRU, NULL, "STASIS_SYSCONFDIR");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_PASSTHRU, NULL, "STASIS_CPU_COUNT");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_REQUIRED | STASIS_ENVCTL_REDACT, callback_except_gh, "STASIS_GH_TOKEN");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_REQUIRED, callback_except_jf, "STASIS_JF_ARTIFACTORY_URL");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_ACCESS_TOKEN");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_PASSTHRU, NULL, "STASIS_JF_USER");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_PASSWORD");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_SSH_KEY_PATH");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_SSH_PASSPHRASE");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_CLIENT_CERT_CERT_PATH");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_CLIENT_CERT_KEY_PATH");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }
    status = envctl_register(&globals.envctl, STASIS_ENVCTL_REQUIRED, callback_except_jf, "STASIS_JF_REPO");
    if (status) {
        SYSERROR("envctl_register failed");
        exit(1);
    }

    envctl_do_required(globals.envctl, globals.verbose);
}

void check_system_requirements(struct Delivery *ctx) {
    const char *tools_required[] = {
        "rsync",
        NULL,
    };

    msg(STASIS_MSG_L1, "Checking system requirements\n");

    msg(STASIS_MSG_L2, "Tools\n");
    for (size_t i = 0; tools_required[i] != NULL; i++) {
        msg(STASIS_MSG_L3, "%s: ", tools_required[i]);
        if (!find_program(tools_required[i])) {
            SYSERROR("'%s' must be installed.", tools_required[i]);
            exit(1);
        }
        msg(STASIS_MSG_RESTRICT, "found\n");
    }

    msg(STASIS_MSG_L2, "Docker\n");
    struct DockerCapabilities *dcap = &ctx->deploy.docker.capabilities;
    docker_capable(dcap);
    if (!globals.enable_docker) {
        dcap->usable = false;
    }
    msg(STASIS_MSG_L3, "Available: %s%s%s\n", dcap->available ? STASIS_COLOR_GREEN : STASIS_COLOR_RED, dcap->available ? "Yes" : "No", STASIS_COLOR_RESET);
    msg(STASIS_MSG_L3, "Usable: %s%s%s %s\n", dcap->usable ? STASIS_COLOR_GREEN : STASIS_COLOR_RED, dcap->usable ? "Yes" : "No", STASIS_COLOR_RESET, globals.enable_docker ? "" : STASIS_COLOR_GREEN "(disabled by CLI argument)" STASIS_COLOR_RESET);
    msg(STASIS_MSG_L3, "Podman [Docker Emulation]: %s\n", dcap->podman ? "Yes" : "No");
    msg(STASIS_MSG_L3, "Build plugin(s): ");
    if (dcap->build) {
        if (dcap->build & STASIS_DOCKER_BUILD) {
            msg(STASIS_MSG_RESTRICT, "build ");
        }
        if (dcap->build & STASIS_DOCKER_BUILD_X) {
            msg(STASIS_MSG_RESTRICT, "buildx ");
        }
        msg(STASIS_MSG_RESTRICT,"\n");
    } else {
        msg(STASIS_MSG_RESTRICT, "%sN/A%s\n", STASIS_COLOR_YELLOW, STASIS_COLOR_RESET);
    }
    if (!dcap->usable) {
        SYSWARN("Docker related tasks are now disabled");
        // disable docker builds
        globals.enable_docker = false;
    }
}

void check_requirements(struct Delivery *ctx) {
    check_system_requirements(ctx);
    check_system_env_requirements();
}

void check_system_path() {
    char *pathvar = NULL;
    pathvar = getenv("PATH");
    if (!pathvar) {
        SYSERROR("PATH variable is not set. Cannot continue.");
        exit(1);
    }
}