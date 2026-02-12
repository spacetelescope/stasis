#include "delivery.h"

void delivery_get_conda_installer_url(struct Delivery *ctx, char *result) {
    if (ctx->conda.installer_version) {
        // Use version specified by configuration file
        sprintf(result, "%s/%s-%s-%s-%s.sh", ctx->conda.installer_baseurl,
                ctx->conda.installer_name,
                ctx->conda.installer_version,
                ctx->conda.installer_platform,
                ctx->conda.installer_arch);
    } else {
        // Use latest installer
        sprintf(result, "%s/%s-%s-%s.sh", ctx->conda.installer_baseurl,
                ctx->conda.installer_name,
                ctx->conda.installer_platform,
                ctx->conda.installer_arch);
    }

}

int delivery_get_conda_installer(struct Delivery *ctx, char *installer_url) {
    char script_path[PATH_MAX];
    char *installer = path_basename(installer_url);

    memset(script_path, 0, sizeof(script_path));
    sprintf(script_path, "%s/%s", ctx->storage.tmpdir, installer);
    if (access(script_path, F_OK)) {
        // Script doesn't exist
        char *errmsg = NULL;
        long fetch_status = download(installer_url, script_path, &errmsg);
        if (HTTP_ERROR(fetch_status) || fetch_status < 0) {
            // download failed
            SYSERROR("download failed: %s: %s\n", errmsg, installer_url);
            guard_free(errmsg);
            return -1;
        }
    } else {
        msg(STASIS_MSG_RESTRICT | STASIS_MSG_L3, "Skipped, installer already exists\n", script_path);
    }

    ctx->conda.installer_path = strdup(script_path);
    if (!ctx->conda.installer_path) {
        SYSERROR("Unable to duplicate script_path: '%s'", script_path);
        return -1;
    }

    return 0;
}

void delivery_install_conda(char *install_script, char *conda_install_dir) {
    struct Process proc = {0};

    if (globals.conda_fresh_start) {
        if (!access(conda_install_dir, F_OK)) {
            // directory exists so remove it
            if (rmtree(conda_install_dir)) {
                perror("unable to remove previous installation");
                exit(1);
            }

            // Proceed with the installation
            // -b = batch mode (non-interactive)
            char cmd[PATH_MAX] = {0};
            snprintf(cmd, sizeof(cmd) - 1, "%s %s -b -p %s",
                     find_program("bash"),
                     install_script,
                     conda_install_dir);
            if (shell_safe(&proc, cmd)) {
                fprintf(stderr, "conda installation failed\n");
                exit(1);
            }
        } else {
            // Proceed with the installation
            // -b = batch mode (non-interactive)
            char cmd[PATH_MAX] = {0};
            snprintf(cmd, sizeof(cmd) - 1, "%s %s -b -p %s",
                     find_program("bash"),
                     install_script,
                     conda_install_dir);
            if (shell_safe(&proc, cmd)) {
                fprintf(stderr, "conda installation failed\n");
                exit(1);
            }
        }
    } else {
        msg(STASIS_MSG_L3, "Conda removal disabled by configuration\n");
    }
}

void delivery_conda_enable(struct Delivery *ctx, char *conda_install_dir) {
    if (conda_activate(conda_install_dir, "base")) {
        fprintf(stderr, "conda activation failed\n");
        exit(1);
    }

    // Setting the CONDARC environment variable appears to be the only consistent
    // way to make sure the file is used. Not setting this variable leads to strange
    // behavior, especially if a conda environment is already active when STASIS is loaded.
    char rcpath[PATH_MAX];
    sprintf(rcpath, "%s/%s", conda_install_dir, ".condarc");
    setenv("CONDARC", rcpath, 1);
    if (runtime_replace(&ctx->runtime.environ, __environ)) {
        perror("unable to replace runtime environment after activating conda");
        exit(1);
    }

    if (conda_setup_headless()) {
        // no COE check. this call must succeed.
        exit(1);
    }
}

