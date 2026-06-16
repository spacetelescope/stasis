#include "delivery.h"
#include "conda.h"

void delivery_get_conda_installer_url(struct Delivery *ctx, char *result, size_t maxlen) {
    int len = 0;
    if (ctx->conda.installer_version) {
        // Use version specified by configuration file
        len = snprintf(NULL, 0, "%s/%s-%s-%s-%s.sh",
                ctx->conda.installer_baseurl,
                ctx->conda.installer_name,
                ctx->conda.installer_version,
                ctx->conda.installer_platform,
                ctx->conda.installer_arch);

        snprintf(result, maxlen - len, "%s/%s-%s-%s-%s.sh",
                ctx->conda.installer_baseurl,
                ctx->conda.installer_name,
                ctx->conda.installer_version,
                ctx->conda.installer_platform,
                ctx->conda.installer_arch);
    } else {
        // Use latest installer
        len = snprintf(NULL, 0, "%s/%s-%s-%s.sh",
                ctx->conda.installer_baseurl,
                ctx->conda.installer_name,
                ctx->conda.installer_platform,
                ctx->conda.installer_arch);

        snprintf(result, maxlen - len, "%s/%s-%s-%s.sh",
                ctx->conda.installer_baseurl,
                ctx->conda.installer_name,
                ctx->conda.installer_platform,
                ctx->conda.installer_arch);
    }
}

int delivery_get_conda_installer(struct Delivery *ctx, char *installer_url) {
    char script_path[PATH_MAX];
    char *installer = path_basename(installer_url);

    memset(script_path, 0, sizeof(script_path));
    snprintf(script_path, sizeof(script_path), "%s/%s", ctx->storage.tmpdir, installer);
    if (access(script_path, F_OK)) {
        // Script doesn't exist
        char *errmsg = NULL;
        long fetch_status = download(installer_url, script_path, &errmsg);
        if (HTTP_ERROR(fetch_status) || fetch_status < 0) {
            // download failed
            SYSERROR("download failed: %s: %s", errmsg, installer_url);
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
                SYSERROR("unable to remove previous installation: %s", strerror(errno));
                exit(1);
            }

            // Proceed with the installation
            // -b = batch mode (non-interactive)
            char cmd[PATH_MAX] = {0};
            snprintf(cmd, sizeof(cmd), "%s %s -b -p %s",
                     find_program("bash"),
                     install_script,
                     conda_install_dir);
            if (shell_safe(&proc, cmd)) {
                SYSERROR("conda installation failed");
                exit(1);
            }
        } else {
            // Proceed with the installation
            // -b = batch mode (non-interactive)
            char cmd[PATH_MAX] = {0};
            snprintf(cmd, sizeof(cmd), "%s %s -b -p %s",
                     find_program("bash"),
                     install_script,
                     conda_install_dir);
            if (shell_safe(&proc, cmd)) {
                SYSERROR("conda installation failed");
                exit(1);
            }
        }
    } else {
        msg(STASIS_MSG_L3, "Conda removal disabled by configuration\n");
    }
}

void delivery_conda_enable(struct Delivery *ctx, char *conda_install_dir) {
    setenv("MAMBA_ROOT_PREFIX", ctx->storage.conda_install_prefix, 1);
    if (conda_activate(conda_install_dir, "base")) {
        SYSERROR("conda activation failed");
        exit(1);
    }

    // Setting the CONDARC environment variable appears to be the only consistent
    // way to make sure the file is used. Not setting this variable leads to strange
    // behavior, especially if a conda environment is already active when STASIS is loaded.
    char rcpath[PATH_MAX];
    snprintf(rcpath, sizeof(rcpath), "%s/%s", conda_install_dir, ".condarc");
    setenv("CONDARC", rcpath, 1);
    setenv("MAMBARC", rcpath, 1);
    if (runtime_replace(&ctx->runtime.environ, __environ)) {
        SYSERROR("unable to replace runtime environment after activating conda");
        exit(1);
    }

    char pinned[PATH_MAX];
    snprintf(pinned, sizeof(pinned), "%s/conda-meta/pinned", conda_install_dir);
    touch(pinned);
    if (errno == ENOENT) {
        errno = 0;
    }

    char *conda_version = strdup(ctx->conda.installer_version);
    if (conda_version) {
        char *rev = strpbrk(conda_version, "-");
        if (rev) {
            *rev = '\0';
        }

        FILE *pinned_fp = fopen(pinned, "w+");
        if (!pinned_fp) {
            SYSERROR("unable to open conda-meta/pinned file for writing: %s", strerror(errno));
            exit(1);
        }
        fprintf(pinned_fp, "conda=%s\n", conda_version);
        fclose(pinned_fp);
        guard_free(conda_version);
    }

    if (conda_capable(&ctx->conda.capabilities, ctx->storage.conda_install_prefix)) {
        SYSERROR("Conda capability check failed");
        exit(1);
    }

    if (!ctx->conda.capabilities.usable) {
        SYSERROR("Conda is broken");
        exit(1);
    }

    if (conda_setup_headless(&ctx->conda.capabilities)) {
        // no COE check. this call must succeed.
        exit(1);
    }
}

