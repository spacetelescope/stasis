#include "system_requirements.h"

void check_system_env_requirements() {
    msg(STASIS_MSG_L1, "Checking environment\n");
    globals.envctl = envctl_init();
    envctl_register(&globals.envctl, STASIS_ENVCTL_PASSTHRU, NULL, "TMPDIR");
    envctl_register(&globals.envctl, STASIS_ENVCTL_PASSTHRU, NULL, "STASIS_ROOT");
    envctl_register(&globals.envctl, STASIS_ENVCTL_PASSTHRU, NULL, "STASIS_SYSCONFDIR");
    envctl_register(&globals.envctl, STASIS_ENVCTL_PASSTHRU, NULL, "STASIS_CPU_COUNT");
    envctl_register(&globals.envctl, STASIS_ENVCTL_REQUIRED | STASIS_ENVCTL_REDACT, callback_except_gh, "STASIS_GH_TOKEN");
    envctl_register(&globals.envctl, STASIS_ENVCTL_REQUIRED, callback_except_jf, "STASIS_JF_ARTIFACTORY_URL");
    envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_ACCESS_TOKEN");
    envctl_register(&globals.envctl, STASIS_ENVCTL_PASSTHRU, NULL, "STASIS_JF_USER");
    envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_PASSWORD");
    envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_SSH_KEY_PATH");
    envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_SSH_PASSPHRASE");
    envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_CLIENT_CERT_CERT_PATH");
    envctl_register(&globals.envctl, STASIS_ENVCTL_REDACT, NULL, "STASIS_JF_CLIENT_CERT_KEY_PATH");
    envctl_register(&globals.envctl, STASIS_ENVCTL_REQUIRED, callback_except_jf, "STASIS_JF_REPO");
    envctl_do_required(globals.envctl, globals.verbose);
}

void check_system_requirements(struct Delivery *ctx) {
    const char *tools_required[] = {
        "rsync",
        NULL,
    };

    msg(STASIS_MSG_L1, "Checking system requirements\n");
    for (size_t i = 0; tools_required[i] != NULL; i++) {
        if (!find_program(tools_required[i])) {
            msg(STASIS_MSG_L2 | STASIS_MSG_ERROR, "'%s' must be installed.\n", tools_required[i]);
            exit(1);
        }
    }

    if (!globals.tmpdir && !ctx->storage.tmpdir) {
        delivery_init_tmpdir(ctx);
    }

    struct DockerCapabilities dcap;
    if (!docker_capable(&dcap)) {
        msg(STASIS_MSG_L2 | STASIS_MSG_WARN, "Docker is broken\n");
        msg(STASIS_MSG_L3, "Available: %s\n", dcap.available ? "Yes" : "No");
        msg(STASIS_MSG_L3, "Usable: %s\n", dcap.usable ? "Yes" : "No");
        msg(STASIS_MSG_L3, "Podman [Docker Emulation]: %s\n", dcap.podman ? "Yes" : "No");
        msg(STASIS_MSG_L3, "Build plugin(s): ");
        if (dcap.usable) {
            if (dcap.build & STASIS_DOCKER_BUILD) {
                printf("build ");
            }
            if (dcap.build & STASIS_DOCKER_BUILD_X) {
                printf("buildx ");
            }
            puts("");
        } else {
            printf("N/A\n");
        }

        // disable docker builds
        globals.enable_docker = false;
    }
}

void check_requirements(struct Delivery *ctx) {
    check_system_requirements(ctx);
    check_system_env_requirements();
}

void check_pathvar(struct Delivery *ctx) {
    char *pathvar = NULL;
    pathvar = getenv("PATH");
    if (!pathvar) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "PATH variable is not set. Cannot continue.\n");
        exit(1);
    } else {
        char pathvar_tmp[STASIS_BUFSIZ];
        sprintf(pathvar_tmp, "%s/bin:%s", ctx->storage.conda_install_prefix, pathvar);
        setenv("PATH", pathvar_tmp, 1);
        pathvar = NULL;
    }
}