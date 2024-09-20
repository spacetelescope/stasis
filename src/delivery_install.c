#include "delivery.h"

static const struct Test *requirement_from_test(struct Delivery *ctx, const char *name) {
    struct Test *result;

    result = NULL;
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        if (ctx->tests[i].name && strstr(name, ctx->tests[i].name)) {
            result = &ctx->tests[i];
            break;
        }
    }
    return result;
}

int delivery_install_packages(struct Delivery *ctx, char *conda_install_dir, char *env_name, int type, struct StrList **manifest) {
    char cmd[PATH_MAX];
    char pkgs[STASIS_BUFSIZ];
    char *env_current = getenv("CONDA_DEFAULT_ENV");

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
                struct Test *info = (struct Test *) requirement_from_test(ctx, name);
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
                        whl = get_wheel_file(ctx->storage.wheel_artifact_dir, info->name,
                                             (char *[]) {ctx->meta.python_compact, ctx->system.arch,
                                                         "none", "any",
                                                         post_commit, hash,
                                                         NULL}, WHEEL_MATCH_ANY);

                        guard_strlist_free(&tag_data);
                        info->version = whl->version;
                        sprintf(cmd + strlen(cmd), " '%s==%s'", info->name, whl->version);
                    } else {
                        sprintf(cmd + strlen(cmd), " '%s==%s'", info->name, info->version);
                    }
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

