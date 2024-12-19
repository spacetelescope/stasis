#include "delivery.h"

static struct Test *requirement_from_test(struct Delivery *ctx, const char *name) {
    struct Test *result = NULL;
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        char *package_name = strdup(name);
        if (package_name) {
            char *spec = find_version_spec(package_name);
            if (spec) {
                *spec = '\0';
            }

            if (ctx->tests[i].name && !strcmp(package_name, ctx->tests[i].name)) {
                result = &ctx->tests[i];
                break;
            }
            guard_free(package_name);
        } else {
            SYSERROR("unable to allocate memory for package name: %s", name);
            return NULL;
        }
    }
    return result;
}

static char *have_spec_in_config(const struct Delivery *ctx, const char *name) {
    for (size_t x = 0; x < strlist_count(ctx->conda.pip_packages); x++) {
        char *config_spec = strlist_item(ctx->conda.pip_packages, x);
        char *op = find_version_spec(config_spec);
        char package[255] = {0};
        if (op) {
            strncpy(package, config_spec, op - config_spec);
        } else {
            strncpy(package, config_spec, sizeof(package) - 1);
        }
        if (strncmp(package, name, strlen(package)) == 0) {
            return config_spec;
        }
    }
    return NULL;
}

int delivery_overlay_packages_from_env(struct Delivery *ctx, const char *env_name) {
    char *current_env = conda_get_active_environment();
    int need_restore = current_env && strcmp(env_name, current_env) != 0;

    conda_activate(ctx->storage.conda_install_prefix, env_name);
    // Retrieve a listing of python packages installed under "env_name"
    int freeze_status = 0;
    char *freeze_output = shell_output("python -m pip freeze", &freeze_status);
    if (freeze_status) {
        guard_free(freeze_output);
        guard_free(current_env);
        return -1;
    }

    if (need_restore) {
        // Restore the original conda environment
        conda_activate(ctx->storage.conda_install_prefix, current_env);
    }
    guard_free(current_env);

    struct StrList *frozen_list = strlist_init();
    strlist_append_tokenize(frozen_list, freeze_output, LINE_SEP);
    guard_free(freeze_output);

    struct StrList *new_list = strlist_init();

    // - consume package specs that have no test blocks.
    // - these will be third-party packages like numpy, scipy, etc.
    // - and they need to be present at the head of the list so they
    //   get installed first.
    for (size_t i = 0; i < strlist_count(ctx->conda.pip_packages); i++) {
        char *spec = strlist_item(ctx->conda.pip_packages, i);
        char spec_name[255] = {0};
        char *op = find_version_spec(spec);
        if (op) {
            strncpy(spec_name, spec, op - spec);
        } else {
            strncpy(spec_name, spec, sizeof(spec_name) - 1);
        }
        struct Test *test_block = requirement_from_test(ctx, spec_name);
        if (!test_block) {
            msg(STASIS_MSG_L2 | STASIS_MSG_WARN, "from config without test: %s\n", spec);
            strlist_append(&new_list, spec);
        }
    }

    // now consume packages that have a test block
    // if the ini provides a spec, override the environment's version.
    // otherwise, use the spec derived from the environment
    for (size_t i = 0; i < strlist_count(frozen_list); i++) {
        char *frozen_spec = strlist_item(frozen_list, i);
        char frozen_name[255] = {0};
        char *op = find_version_spec(frozen_spec);
        // we only care about packages with specs here. if something else arrives, ignore it
        if (op) {
            strncpy(frozen_name, frozen_spec, op - frozen_spec);
        } else {
            strncpy(frozen_name, frozen_spec, sizeof(frozen_name) - 1);
        }
        struct Test *test = requirement_from_test(ctx, frozen_name);
        if (test && strcmp(test->name, frozen_name) == 0) {
            char *config_spec = have_spec_in_config(ctx, frozen_name);
            if (config_spec) {
                msg(STASIS_MSG_L2, "from config: %s\n", config_spec);
                strlist_append(&new_list, config_spec);
            } else {
                msg(STASIS_MSG_L2, "from environment: %s\n", frozen_spec);
                strlist_append(&new_list, frozen_spec);
            }
        }
    }

    // Replace the package manifest as needed
    if (strlist_count(new_list)) {
        guard_strlist_free(&ctx->conda.pip_packages);
        ctx->conda.pip_packages = strlist_copy(new_list);
    }
    guard_strlist_free(&new_list);
    guard_strlist_free(&frozen_list);
    return 0;
}

int delivery_purge_packages(struct Delivery *ctx, const char *env_name, int use_pkg_manager) {
    int status = 0;
    char subcommand[100] = {0};
    char package_manager[100] = {0};
    typedef int (fnptr)(const char *);

    const char *current_env = conda_get_active_environment();
    if (current_env) {
        conda_activate(ctx->storage.conda_install_prefix, env_name);
    }

    struct StrList *list = NULL;
    fnptr *fn = NULL;
    switch (use_pkg_manager) {
        case PKG_USE_CONDA:
            fn = conda_exec;
            list = ctx->conda.conda_packages_purge;
            strcpy(package_manager, "conda");
            // conda is already configured for "always_yes"
            strcpy(subcommand, "remove");
            break;
        case PKG_USE_PIP:
            fn = pip_exec;
            list = ctx->conda.pip_packages_purge;
            strcpy(package_manager, "pip");
            // avoid user prompt to remove packages
            strcpy(subcommand, "uninstall -y");
            break;
        default:
            SYSERROR("Unknown package manager: %d", use_pkg_manager);
            status = -1;
            break;
    }

    for (size_t i = 0; i < strlist_count(list); i++) {
        char *package = strlist_item(list, i);
        char *command = NULL;
        if (asprintf(&command, "%s '%s'", subcommand, package) < 0) {
            SYSERROR("%s", "Unable to allocate bytes for removal command");
            status = -1;
            break;
        }

        if (fn(command)) {
            SYSERROR("%s removal operation failed", package_manager);
            guard_free(command);
            status = 1;
            break;
        }
    }

    if (current_env) {
        conda_activate(ctx->storage.conda_install_prefix, current_env);
    }

    return status;
}

int delivery_install_packages(struct Delivery *ctx, char *conda_install_dir, char *env_name, int type, struct StrList **manifest) {
    char cmd[PATH_MAX];
    char pkgs[STASIS_BUFSIZ];
    const char *env_current = getenv("CONDA_DEFAULT_ENV");

    if (env_current) {
        // The requested environment is not the current environment
        if (strcmp(env_current, env_name) != 0) {
            // Activate the requested environment
            printf("Activating: %s\n", env_name);
            conda_activate(conda_install_dir, env_name);
            runtime_replace(&ctx->runtime.environ, __environ);
        }
    }

    memset(cmd, 0, sizeof(cmd));
    memset(pkgs, 0, sizeof(pkgs));
    strcat(cmd, "install");

    typedef int (*Runner)(const char *);
    Runner runner = NULL;
    if (INSTALL_PKG_CONDA & type) {
        runner = conda_exec;
    } else if (INSTALL_PKG_PIP & type) {
        runner = pip_exec;
    }

    if (INSTALL_PKG_CONDA_DEFERRED & type) {
        strcat(cmd, " --use-local");
    } else if (INSTALL_PKG_PIP_DEFERRED & type) {
        // Don't change the baseline package set unless we're working with a
        // new build. Release candidates will need to keep packages as stable
        // as possible between releases.
        if (!ctx->meta.based_on) {
            strcat(cmd, " --upgrade");
        }
        sprintf(cmd + strlen(cmd), " --extra-index-url 'file://%s'", ctx->storage.wheel_artifact_dir);
    }

    for (size_t x = 0; manifest[x] != NULL; x++) {
        char *name = NULL;
        for (size_t p = 0; p < strlist_count(manifest[x]); p++) {
            name = strlist_item(manifest[x], p);
            strip(name);
            if (!strlen(name)) {
                continue;
            }
            if (INSTALL_PKG_PIP_DEFERRED & type) {
                struct Test *info = requirement_from_test(ctx, name);
                if (info) {
                    if (!strcmp(info->version, "HEAD")) {
                        struct StrList *tag_data = strlist_init();
                        if (!tag_data) {
                            SYSERROR("%s", "Unable to allocate memory for tag data\n");
                            return -1;
                        }
                        strlist_append_tokenize(tag_data, info->repository_info_tag, "-");

                        struct Wheel *whl = NULL;
                        char *post_commit = NULL;
                        char *hash = NULL;
                        if (strlist_count(tag_data) > 1) {
                            post_commit = strlist_item(tag_data, 1);
                            hash = strlist_item(tag_data, 2);
                        }

                        // We can't match on version here (index 0). The wheel's version is not guaranteed to be
                        // equal to the tag; setuptools_scm auto-increments the value, the user can change it manually,
                        // etc.
                        errno = 0;
                        whl = get_wheel_info(ctx->storage.wheel_artifact_dir, info->name,
                                             (char *[]) {ctx->meta.python_compact, ctx->system.arch,
                                                         "none", "any",
                                                         post_commit, hash,
                                                         NULL}, WHEEL_MATCH_ANY);
                        if (!whl && errno) {
                            // error
                            SYSERROR("Unable to read Python wheel info: %s\n", strerror(errno));
                            exit(1);
                        } else if (!whl) {
                            // not found
                            fprintf(stderr, "No wheel packages found that match the description of '%s'", info->name);
                        } else {
                            // found
                            guard_strlist_free(&tag_data);
                            info->version = strdup(whl->version);
                        }
                        wheel_free(&whl);
                    }
                    snprintf(cmd + strlen(cmd),
                             sizeof(cmd) - strlen(cmd) - strlen(info->name) - strlen(info->version) + 5,
                             " '%s==%s'", info->name, info->version);
                } else {
                    fprintf(stderr, "Deferred package '%s' is not present in the tested package list!\n", name);
                    return -1;
                }
            } else {
                if (startswith(name, "--") || startswith(name, "-")) {
                    sprintf(cmd + strlen(cmd), " %s", name);
                } else {
                    sprintf(cmd + strlen(cmd), " '%s'", name);
                }
            }
        }
        int status = runner(cmd);
        if (status) {
            return status;
        }
    }
    return 0;
}

