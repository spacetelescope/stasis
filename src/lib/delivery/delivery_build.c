#include <fnmatch.h>

#include "delivery.h"
#include "conda.h"
#include "recipe.h"

int delivery_build_recipes(struct Delivery *ctx) {
    for (size_t i = 0; i < ctx->tests->num_used; i++) {
        char *recipe_dir = NULL;
        if (ctx->tests->test[i]->build_recipe) { // build a conda recipe
            if (recipe_clone(ctx->storage.build_recipes_dir, ctx->tests->test[i]->build_recipe, NULL, &recipe_dir)) {
                SYSERROR("Encountered an issue while cloning recipe for: %s", ctx->tests->test[i]->name);
                return -1;
            }
            if (!recipe_dir) {
                SYSERROR("BUG: recipe_clone() succeeded but recipe_dir is NULL: %s", strerror(errno));
                return -1;
            }
            const int recipe_style = recipe_get_style(recipe_dir);
            const int recipe_build_system = recipe_get_build_system(recipe_dir, recipe_style);

            if(!pushd(recipe_dir)) {
                if (RECIPE_STYLE_ASTROCONDA == recipe_style) {
                    pushd(path_basename(ctx->tests->test[i]->repository));
                } else if (RECIPE_STYLE_CONDA_FORGE == recipe_style) {
                    pushd("recipe");
                }

                char recipe_version[200];
                char recipe_buildno[200];
                char recipe_git_url[PATH_MAX];
                char recipe_git_rev[PATH_MAX];

                char tag[100] = {0};
                if (ctx->tests->test[i]->repository_info_tag) {
                    const int is_long_tag = num_chars(ctx->tests->test[i]->repository_info_tag, '-') > 1;
                    if (is_long_tag) {
                        const size_t len = strcspn(ctx->tests->test[i]->repository_info_tag, "-");
                        safe_strncpy(tag, ctx->tests->test[i]->repository_info_tag, len);
                    } else {
                        safe_strncpy(tag, ctx->tests->test[i]->repository_info_tag, sizeof(tag));
                    }
                } else {
                    safe_strncpy(tag, ctx->tests->test[i]->version, sizeof(tag));
                }

                //sprintf(recipe_version, "{%% set version = GIT_DESCRIBE_TAG ~ \".dev\" ~ GIT_DESCRIBE_NUMBER ~ \"+\" ~ GIT_DESCRIBE_HASH %%}");
                //sprintf(recipe_git_url, "  git_url: %s", ctx->tests->test[i]->repository);
                //sprintf(recipe_git_rev, "  git_rev: %s", ctx->tests->test[i]->version);
                // TODO: Conditionally download archives if github.com is the origin. Else, use raw git_* keys ^^^
                //       03/2026 - How can we know if the repository URL supports archive downloads?
                //                 Perhaps we can key it to the recipe type, because the archive is a requirement imposed
                //                 by conda-forge. Hmm.

                snprintf(recipe_version, sizeof(recipe_version), "{%% set version = \"%s\" %%}", tag);
                snprintf(recipe_git_url, sizeof(recipe_git_url), "  url: %s/archive/refs/tags/{{ version }}.tar.gz", ctx->tests->test[i]->repository);
                safe_strncpy(recipe_git_rev, "", sizeof(recipe_git_rev));
                snprintf(recipe_buildno, sizeof(recipe_buildno), "  number: 0");

                unsigned flags = REPLACE_TRUNCATE_AFTER_MATCH;
                if (recipe_build_system == RECIPE_BUILD_CONDA_BUILD) {
                    if (ctx->meta.final) { // remove this. i.e. statis cannot deploy a release to conda-forge
                        snprintf(recipe_version, sizeof(recipe_version), "{%% set version = \"%s\" %%}", ctx->tests->test[i]->version);
                        // TODO: replace sha256 of tagged archive
                        // TODO: leave the recipe unchanged otherwise. in theory this should produce the same conda package hash as conda forge.
                        // For now, remove the sha256 requirement
                        file_replace_text("meta.yaml", "sha256:", "\n", flags);
                    } else {
                        file_replace_text("meta.yaml", "{% set version = ", recipe_version, flags);
                        file_replace_text("meta.yaml", "  url:", recipe_git_url, flags);
                        file_replace_text("meta.yaml", "  sha256:", "\n", flags);
                        file_replace_text("meta.yaml", "  number:", recipe_buildno, flags);
                    }
                } else if (recipe_build_system == RECIPE_BUILD_RATTLER) {
                    file_replace_text("recipe.yaml", "  version:", ctx->tests->test[i]->version, flags);
                    file_replace_text("recipe.yaml", "  url:", recipe_git_url, flags);
                    file_replace_text("recipe.yaml", "  sha256:", "\n", flags);
                    file_replace_text("recipe.yaml", "  number:", recipe_buildno, flags);
                }

                char command[PATH_MAX];
                if (RECIPE_STYLE_CONDA_FORGE == recipe_style) {
                    char arch[STASIS_NAME_MAX] = {0};
                    char platform[STASIS_NAME_MAX] = {0};

                    safe_strncpy(platform, ctx->system.platform[DELIVERY_PLATFORM], sizeof(platform));
                    if (strstr(platform, "Darwin")) {
                        memset(platform, 0, sizeof(platform));
                        safe_strncpy(platform, "osx", sizeof(platform));
                    }
                    tolower_s(platform);

                    if (strstr(ctx->system.arch, "arm64")) {
                        safe_strncpy(arch, "arm64", sizeof(arch));
                    } else if (strstr(ctx->system.arch, "64")) {
                        safe_strncpy(arch, "64", sizeof(arch));
                    } else {
                        safe_strncat(arch, "32", sizeof(arch)); // blind guess
                    }
                    tolower_s(arch);

                    // default build tool is "conda build" aka "build"
                    char tool[STASIS_NAME_MAX] = "build";
                    if (recipe_build_system == RECIPE_BUILD_CONDA_BUILD) {
                        if (strlist_contains(globals.conda_packages, "boa", NULL)) {
                            safe_strncpy(tool, "mambabuild", sizeof(tool));
                        }
                    } else if (recipe_build_system == RECIPE_BUILD_RATTLER) {
                        snprintf(tool, sizeof(tool), "rattler-build");
                    }
                    snprintf(command, sizeof(command), "%s --python=%s -m ../.ci_support/%s_%s_.yaml .",
                            tool, ctx->meta.python, platform, arch);
                } else {
                    snprintf(command, sizeof(command), "build --python=%s .", ctx->meta.python);
                }

                int status = 0;
                if (recipe_build_system == RECIPE_BUILD_RATTLER) {
                    // rattler-build is a standalone program, not a conda sub-command
                    status = system(command);
                } else {
                    status = conda_exec(command);
                }
                if (status) {
                    guard_free(recipe_dir);
                    return -1;
                }

                if (RECIPE_STYLE_GENERIC != recipe_style) {
                    popd();
                }
                popd();
            } else {
                SYSERROR("Unable to enter recipe directory %s: %s", recipe_dir, strerror(errno));
                guard_free(recipe_dir);
                return -1;
            }
        }
        guard_free(recipe_dir);
    }
    return 0;
}

int filter_repo_tags(char *repo, struct StrList *patterns) {
    int result = 0;

    if (!pushd(repo)) {
        int list_status = 0;
        char *tags_raw = shell_output("git tag -l", &list_status);
        struct StrList *tags = strlist_init();
        strlist_append_tokenize(tags, tags_raw, LINE_SEP);

        for (size_t i = 0; tags && i < strlist_count(tags); i++) {
            char *tag = strlist_item(tags, i);
            for (size_t p = 0; p < strlist_count(patterns); p++) {
                char *pattern = strlist_item(patterns, p);
                int match = fnmatch(pattern, tag, 0);
                if (!match) {
                    char cmd[PATH_MAX] = {0};
                    snprintf(cmd, sizeof(cmd), "git tag -d %s", tag);
                    result += system(cmd);
                    break;
                }
            }
        }
        guard_strlist_free(&tags);
        guard_free(tags_raw);
        popd();
    } else {
        result = -1;
    }
    return result;
}

static int read_without_line_endings(const size_t line, char ** arg) {
    (void) line;
    if (*arg) {
        strip(*arg);
        if (isempty(*arg)) {
            return 1; // skip
        }
    }
    return 0;
}

int manylinux_exec(const char *image, const char *script, const char *copy_to_container_dir, const char *copy_from_container_dir, const char *copy_to_host_dir) {
    int result = -1; // fail by default
    char *container_name = NULL;
    char *source_copy_command = NULL;
    char *copy_command = NULL;
    char *rm_command = NULL;
    char *nop_create_command = NULL;
    char *nop_rm_command = NULL;
    char *volume_rm_command = NULL;
    char *find_command = NULL;
    char *wheel_paths_filename = NULL;
    char *args = NULL;

    const uid_t uid = geteuid();
    char suffix[7] = {0};

    // setup

    if (get_random_bytes(suffix, sizeof(suffix))) {
        SYSERROR("unable to acquire value from random generator");
        goto manylinux_fail;
    }

    if (asprintf(&container_name, "manylinux_build_%d_%zd_%s", uid, time(NULL), suffix) < 0) {
        SYSERROR("unable to allocate memory for container name");
        goto manylinux_fail;
    }

    if (asprintf(&args, "--name %s -w /build -v %s:/build", container_name, container_name) < 0) {
        SYSERROR("unable to allocate memory for docker arguments");
        goto manylinux_fail;
    }

    if (!strstr(image, "manylinux")) {
        SYSERROR("expected a manylinux image, but got %s", image);
        goto manylinux_fail;
    }

    if (asprintf(&nop_create_command, "run --name nop_%s -v %s:/build busybox", container_name, container_name) < 0) {
        SYSERROR("unable to allocate memory for nop container command");
        goto manylinux_fail;
    }

    if (asprintf(&source_copy_command, "cp %s nop_%s:/build", copy_to_container_dir, container_name) < 0) {
        SYSERROR("unable to allocate memory for source copy command");
        goto manylinux_fail;
    }

    if (asprintf(&nop_rm_command, "rm nop_%s", container_name) < 0) {
        SYSERROR("unable to allocate memory for nop container command");
        goto manylinux_fail;
    }

    if (asprintf(&wheel_paths_filename, "%s/wheel_paths_%s.txt", globals.tmpdir, container_name) < 0) {
        SYSERROR("unable to allocate memory for wheel paths file name");
        goto manylinux_fail;
    }

    if (asprintf(&find_command, "run --rm -t -v %s:/build busybox sh -c 'find %s -name \"*.whl\"' > %s", container_name, copy_from_container_dir, wheel_paths_filename) < 0) {
        SYSERROR("unable to allocate memory for find command");
        goto manylinux_fail;
    }

    // execute

    if (docker_exec(nop_create_command, 0)) {
        SYSERROR("docker nop container creation failed");
        goto manylinux_fail;
    }

    if (docker_exec(source_copy_command, 0)) {
        SYSERROR("docker source copy operation failed");
        goto manylinux_fail;
    }

    if (docker_exec(nop_rm_command, STASIS_DOCKER_QUIET)) {
        SYSERROR("docker nop container removal failed");
        goto manylinux_fail;
    }

    if (docker_script(image, args, (char *) script, 0)) {
        SYSERROR("manylinux execution failed");
        goto manylinux_fail;
    }

    if (docker_exec(find_command, 0)) {
        SYSERROR("docker find command failed");
        goto manylinux_fail;
    }

    struct StrList *wheel_paths = strlist_init();
    if (!wheel_paths) {
        SYSERROR("wheel_paths not initialized");
        goto manylinux_fail;
    }

    if (strlist_append_file(wheel_paths, wheel_paths_filename, read_without_line_endings)) {
        SYSERROR("wheel_paths append failed");
        goto manylinux_fail;
    }

    for (size_t i = 0; i < strlist_count(wheel_paths); i++) {
        const char *item = strlist_item(wheel_paths, i);
        if (asprintf(&copy_command, "cp %s:%s %s", container_name, item, copy_to_host_dir) < 0) {
            SYSERROR("unable to allocate memory for docker copy command");
            goto manylinux_fail;
        }

        if (docker_exec(copy_command, 0)) {
            SYSERROR("docker copy operation failed");
            goto manylinux_fail;
        }
        guard_free(copy_command);
    }

    // Success
    result = 0;

    manylinux_fail:
    if (wheel_paths_filename) {
        remove(wheel_paths_filename);
    }

    if (container_name) {
        // Keep going on failure unless memory related.
        // We don't want build debris everywhere.
        if (asprintf(&rm_command, "rm %s", container_name) < 0) {
            SYSERROR("unable to allocate memory for rm command");
            goto late_fail;
        }

        if (docker_exec(rm_command, STASIS_DOCKER_QUIET)) {
            SYSERROR("docker container removal operation failed");
        }

        if (asprintf(&volume_rm_command, "volume rm -f %s", container_name) < 0) {
            SYSERROR("unable to allocate memory for docker volume removal command");
            goto late_fail;
        }

        if (docker_exec(volume_rm_command, STASIS_DOCKER_QUIET)) {
            SYSERROR("docker volume removal operation failed");
        }
    }

    late_fail:
    guard_free(container_name);
    guard_free(args);
    guard_free(copy_command);
    guard_free(rm_command);
    guard_free(volume_rm_command);
    guard_free(source_copy_command);
    guard_free(nop_create_command);
    guard_free(nop_rm_command);
    guard_free(find_command);
    guard_free(wheel_paths_filename);
    guard_strlist_free(&wheel_paths);
    return result;
}

int delivery_build_wheels_manylinux(struct Delivery *ctx, const char *outdir) {
    msg(STASIS_MSG_L1, "Building wheels\n");

    const char *manylinux_image = globals.wheel_builder_manylinux_image;
    if (!manylinux_image) {
        SYSERROR("manylinux_image not initialized");
        return -1;
    }

    int manylinux_build_status = 0;

    msg(STASIS_MSG_L2, "Using: %s\n", manylinux_image);
    const struct Meta *meta = &ctx->meta;
    const char *script_fmt =
        "set -e -x\n"
        "git config --global --add safe.directory /build\n"
        "python%s -m pip install auditwheel build\n"
        "python%s -m build -w .\n"
        "auditwheel show --allow-pure-python-wheel dist/*.whl\n"
        "auditwheel repair --allow-pure-python-wheel dist/*.whl\n";
    char *script = NULL;
    if (asprintf(&script, script_fmt,
        meta->python, meta->python) < 0) {
        SYSERROR("unable to allocate memory for build script");
        return -1;
    }
    manylinux_build_status = manylinux_exec(
        manylinux_image,
        script,
        "./",
        "/build/wheelhouse",
        outdir);

    if (manylinux_build_status) {
        SYSERROR("manylinux build failed (%d)", manylinux_build_status);
        guard_free(script);
        return -1;
    }
    guard_free(script);
    return 0;
}

struct StrList *delivery_build_wheels(struct Delivery *ctx) {
    const int on_linux = strcmp(ctx->system.platform[DELIVERY_PLATFORM], "Linux") == 0;
    const int docker_usable = ctx->deploy.docker.capabilities.usable;
    int use_builder_build = strcmp(globals.wheel_builder, "native") == 0;
    const int use_builder_cibuildwheel = strcmp(globals.wheel_builder, "cibuildwheel") == 0 && on_linux && docker_usable;
    const int use_builder_manylinux = strcmp(globals.wheel_builder, "manylinux") == 0 && on_linux && docker_usable;

    if (!use_builder_build && !use_builder_cibuildwheel && !use_builder_manylinux) {
        SYSWARN("Cannot build wheel for platform using: %s", globals.wheel_builder);
        SYSWARN("Falling back to native toolchain.", globals.wheel_builder);
        use_builder_build = 1;
    }

    struct StrList *result = NULL;
    struct Process proc = {0};

    result = strlist_init();
    if (!result) {
        SYSERROR("unable to allocate memory for string list");
        return NULL;
    }

    for (size_t p = 0; p < strlist_count(ctx->conda.pip_packages_defer); p++) {
        char name[100] = {0};
        char *fullspec = strlist_item(ctx->conda.pip_packages_defer, p);
        safe_strncpy(name, fullspec, sizeof(name));
        remove_extras(name);
        char *spec = find_version_spec(name);
        if (spec) {
            *spec = '\0';
        }

        for (size_t i = 0; i < ctx->tests->num_used; i++) {
            if ((ctx->tests->test[i]->name && !strcmp(name, ctx->tests->test[i]->name)) && (!ctx->tests->test[i]->build_recipe && ctx->tests->test[i]->repository)) { // build from source
                char srcdir[PATH_MAX];
                char wheeldir[PATH_MAX];
                memset(srcdir, 0, sizeof(srcdir));
                memset(wheeldir, 0, sizeof(wheeldir));

                snprintf(srcdir, sizeof(srcdir), "%s/%s", ctx->storage.build_sources_dir, ctx->tests->test[i]->name);
                if (git_clone(&proc, ctx->tests->test[i]->repository, srcdir, ctx->tests->test[i]->version)) {
                    SYSERROR("Unable to checkout tag '%s' for package '%s' from repository '%s'",
                    ctx->tests->test[i]->version, ctx->tests->test[i]->name, ctx->tests->test[i]->repository);
                    return NULL;
                }

                if (!ctx->tests->test[i]->repository_info_tag) {
                    ctx->tests->test[i]->repository_info_tag = strdup(git_describe(srcdir));
                }
                if (!ctx->tests->test[i]->repository_info_ref) {
                    ctx->tests->test[i]->repository_info_ref = strdup(git_rev_parse(srcdir, ctx->tests->test[i]->version));
                }
                if (ctx->tests->test[i]->repository_remove_tags && strlist_count(ctx->tests->test[i]->repository_remove_tags)) {
                    filter_repo_tags(srcdir, ctx->tests->test[i]->repository_remove_tags);
                }

                if (!pushd(srcdir)) {
                    char dname[NAME_MAX];
                    char outdir[PATH_MAX];
                    char *cmd = NULL;
                    memset(dname, 0, sizeof(dname));
                    memset(outdir, 0, sizeof(outdir));

                    const int dep_status = check_python_package_dependencies(".");
                    if (dep_status) {
                        SYSERROR("Please replace all occurrences above with standard package specs:\n"
                                        "\n"
                                        "    package==x.y.z\n"
                                        "    package>=x.y.z\n"
                                        "    package<=x.y.z\n"
                                        "    ...\n"
                                        "\n");
                        COE_CHECK_ABORT(true, "Unreproducible delivery");
                    }

                    safe_strncpy(dname, ctx->tests->test[i]->name, sizeof(dname));
                    tolower_s(dname);
                    snprintf(outdir, sizeof(outdir), "%s/%s", ctx->storage.wheel_artifact_dir, dname);
                    if (mkdirs(outdir, 0755)) {
                        SYSERROR("failed to create output directory: %s", outdir);
                        guard_strlist_free(&result);
                        return NULL;
                    }
                    if (use_builder_manylinux) {
                        if (delivery_build_wheels_manylinux(ctx, outdir)) {
                            SYSERROR("failed to generate wheel package for %s-%s", ctx->tests->test[i]->name,
                                    ctx->tests->test[i]->version);
                            guard_strlist_free(&result);
                            guard_free(cmd);
                            return NULL;
                        }
                    } else if (use_builder_build || use_builder_cibuildwheel) {
                        if (use_builder_build) {
                            if (asprintf(&cmd, "-m build -w -o %s", outdir) < 0) {
                                SYSERROR("Unable to allocate memory for build command");
                                return NULL;
                            }
                        } else if (use_builder_cibuildwheel) {
                            if (asprintf(&cmd, "-m cibuildwheel --output-dir %s --only cp%s-manylinux_%s",
                                outdir, ctx->meta.python_compact, ctx->system.arch) < 0) {
                                SYSERROR("Unable to allocate memory for cibuildwheel command");
                                return NULL;
                            }
                        }

                        if (python_exec(cmd)) {
                            SYSERROR("failed to generate wheel package for %s-%s", ctx->tests->test[i]->name,
                                    ctx->tests->test[i]->version);
                            guard_strlist_free(&result);
                            guard_free(cmd);
                            return NULL;
                        }
                    } else {
                        SYSERROR("unknown wheel builder backend: %s", globals.wheel_builder);
                        return NULL;
                    }

                    guard_free(cmd);
                    popd();
                } else {
                    SYSERROR("Unable to enter source directory %s: %s", srcdir, strerror(errno));
                    guard_strlist_free(&result);
                    return NULL;
                }
            }
        }
    }
    return result;
}
