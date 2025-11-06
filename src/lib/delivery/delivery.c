#include "delivery.h"

static char *strdup_maybe(const char * restrict s) {
    if (s != NULL) {
        return strdup(s);
    }
    return NULL;
}
struct Delivery *delivery_duplicate(const struct Delivery *ctx) {
    struct Delivery *result = calloc(1, sizeof(*result));
    if (!result) {
        return NULL;
    }
    // Conda
    result->conda.conda_packages = strlist_copy(ctx->conda.conda_packages);
    result->conda.conda_packages_defer = strlist_copy(ctx->conda.conda_packages_defer);
    result->conda.conda_packages_purge = strlist_copy(ctx->conda.conda_packages_purge);
    result->conda.pip_packages = strlist_copy(ctx->conda.pip_packages);
    result->conda.pip_packages_defer = strlist_copy(ctx->conda.pip_packages_defer);
    result->conda.pip_packages_purge = strlist_copy(ctx->conda.pip_packages_purge);
    result->conda.wheels_packages = strlist_copy(ctx->conda.wheels_packages);
    result->conda.installer_arch = strdup_maybe(ctx->conda.installer_arch);
    result->conda.installer_baseurl = strdup_maybe(ctx->conda.installer_baseurl);
    result->conda.installer_name = strdup_maybe(ctx->conda.installer_name);
    result->conda.installer_path = strdup_maybe(ctx->conda.installer_path);
    result->conda.installer_platform = strdup_maybe(ctx->conda.installer_platform);
    result->conda.installer_version = strdup_maybe(ctx->conda.installer_version);
    result->conda.tool_build_version = strdup_maybe(ctx->conda.tool_build_version);
    result->conda.tool_version = strdup_maybe(ctx->conda.tool_version);

    // Docker
    result->deploy.docker.build_args = strlist_copy(ctx->deploy.docker.build_args);
    result->deploy.docker.tags = strlist_copy(ctx->deploy.docker.tags);
    result->deploy.docker.capabilities = ctx->deploy.docker.capabilities;
    result->deploy.docker.dockerfile = strdup_maybe(ctx->deploy.docker.dockerfile);
    result->deploy.docker.image_compression = strdup_maybe(ctx->deploy.docker.image_compression);
    result->deploy.docker.registry = strdup_maybe(ctx->deploy.docker.registry);
    result->deploy.docker.test_script = strdup_maybe(ctx->deploy.docker.test_script);

    // Info
    result->info.build_name = strdup_maybe(ctx->info.build_name);
    result->info.build_number = strdup_maybe(ctx->info.build_number);
    result->info.release_name = strdup_maybe(ctx->info.release_name);
    result->info.time_info = ctx->info.time_info;
    result->info.time_now = ctx->info.time_now;
    result->info.time_str_epoch = strdup_maybe(ctx->info.time_str_epoch);

    // Meta
    result->meta.name = strdup_maybe(ctx->meta.name);
    result->meta.based_on = strdup_maybe(ctx->meta.based_on);
    result->meta.codename = strdup_maybe(ctx->meta.codename);
    result->meta.mission = strdup_maybe(ctx->meta.mission);
    result->meta.final = ctx->meta.final;
    result->meta.python = strdup_maybe(ctx->meta.python);
    result->meta.python_compact = strdup_maybe(ctx->meta.python_compact);
    result->meta.rc = ctx->meta.rc;
    result->meta.version = strdup_maybe(ctx->meta.version);

    // Rules
    result->rules.build_name_fmt = strdup_maybe(ctx->rules.build_name_fmt);
    result->rules.build_number_fmt = strdup_maybe(ctx->rules.build_number_fmt);
    // Unused member?
    result->rules.enable_final = ctx->rules.enable_final;
    result->rules.release_fmt = ctx->rules.release_fmt;
    // TODO: need content duplication function
    memcpy(&result->rules.content, &ctx->rules.content, sizeof(ctx->rules.content));

    if (ctx->rules._handle) {
        result->rules._handle = malloc(sizeof(*result->rules._handle));
        result->rules._handle->section = malloc(result->rules._handle->section_count * sizeof(*result->rules._handle->section));
        memcpy(result->rules._handle, &ctx->rules._handle, sizeof(*ctx->rules._handle));
    }

    // Runtime
    if (ctx->runtime.environ) {
        result->runtime.environ = runtime_copy(ctx->runtime.environ->data);
    }

    // Storage
    result->storage.tools_dir = strdup_maybe(ctx->storage.tools_dir);
    result->storage.package_dir = strdup_maybe(ctx->storage.package_dir);
    result->storage.results_dir = strdup_maybe(ctx->storage.results_dir);
    result->storage.output_dir = strdup_maybe(ctx->storage.output_dir);
    result->storage.cfgdump_dir = strdup_maybe(ctx->storage.cfgdump_dir);
    result->storage.delivery_dir = strdup_maybe(ctx->storage.delivery_dir);
    result->storage.meta_dir = strdup_maybe(ctx->storage.meta_dir);
    result->storage.mission_dir = strdup_maybe(ctx->storage.mission_dir);
    result->storage.root = strdup_maybe(ctx->storage.root);
    result->storage.tmpdir = strdup_maybe(ctx->storage.tmpdir);
    result->storage.build_dir = strdup_maybe(ctx->storage.build_dir);
    result->storage.build_docker_dir = strdup_maybe(ctx->storage.build_docker_dir);
    result->storage.build_recipes_dir = strdup_maybe(ctx->storage.build_recipes_dir);
    result->storage.build_sources_dir = strdup_maybe(ctx->storage.build_sources_dir);
    result->storage.build_testing_dir = strdup_maybe(ctx->storage.build_testing_dir);
    result->storage.conda_artifact_dir = strdup_maybe(ctx->storage.conda_artifact_dir);
    result->storage.conda_install_prefix = strdup_maybe(ctx->storage.conda_install_prefix);
    result->storage.conda_staging_dir = strdup_maybe(ctx->storage.conda_staging_dir);
    result->storage.conda_staging_url = strdup_maybe(ctx->storage.conda_staging_url);
    result->storage.docker_artifact_dir = strdup_maybe(ctx->storage.docker_artifact_dir);
    result->storage.wheel_artifact_dir = strdup_maybe(ctx->storage.wheel_artifact_dir);
    result->storage.wheel_staging_url = strdup_maybe(ctx->storage.wheel_staging_url);

    result->system.arch = strdup_maybe(ctx->system.arch);
    if (ctx->system.platform) {
        result->system.platform = malloc(DELIVERY_PLATFORM_MAX * sizeof(*result->system.platform));
        for (size_t i = 0; i < DELIVERY_PLATFORM_MAX; i++) {
            result->system.platform[i] = strdup_maybe(ctx->system.platform[i]);
        }
    }

    // Jfrog
    // TODO: break out into a separate a function
    for (size_t i = 0; i < sizeof(ctx->deploy.jfrog) / sizeof(ctx->deploy.jfrog[0]); i++) {
        result->deploy.jfrog[i].dest = strdup_maybe(ctx->deploy.jfrog[i].dest);
        result->deploy.jfrog[i].files = strlist_copy(ctx->deploy.jfrog[i].files);
        result->deploy.jfrog[i].repo = strdup_maybe(ctx->deploy.jfrog[i].repo);
        result->deploy.jfrog[i].upload_ctx.ant = ctx->deploy.jfrog[i].upload_ctx.ant;
        result->deploy.jfrog[i].upload_ctx.archive = ctx->deploy.jfrog[i].upload_ctx.archive;
        result->deploy.jfrog[i].upload_ctx.build_name = ctx->deploy.jfrog[i].upload_ctx.build_name;
        result->deploy.jfrog[i].upload_ctx.build_number = ctx->deploy.jfrog[i].upload_ctx.build_number;
        result->deploy.jfrog[i].upload_ctx.deb = ctx->deploy.jfrog[i].upload_ctx.deb;
        result->deploy.jfrog[i].upload_ctx.detailed_summary = ctx->deploy.jfrog[i].upload_ctx.detailed_summary;
        result->deploy.jfrog[i].upload_ctx.dry_run = ctx->deploy.jfrog[i].upload_ctx.dry_run;
        result->deploy.jfrog[i].upload_ctx.exclusions = strdup_maybe(ctx->deploy.jfrog[i].upload_ctx.exclusions);
        result->deploy.jfrog[i].upload_ctx.explode = ctx->deploy.jfrog[i].upload_ctx.explode;
        result->deploy.jfrog[i].upload_ctx.fail_no_op = ctx->deploy.jfrog[i].upload_ctx.fail_no_op;
        result->deploy.jfrog[i].upload_ctx.flat = ctx->deploy.jfrog[i].upload_ctx.flat;
        result->deploy.jfrog[i].upload_ctx.include_dirs = ctx->deploy.jfrog[i].upload_ctx.include_dirs;
        result->deploy.jfrog[i].upload_ctx.module = strdup_maybe(ctx->deploy.jfrog[i].upload_ctx.module);
        result->deploy.jfrog[i].upload_ctx.project = strdup_maybe(ctx->deploy.jfrog[i].upload_ctx.project);
        result->deploy.jfrog[i].upload_ctx.quiet = ctx->deploy.jfrog[i].upload_ctx.quiet;
        result->deploy.jfrog[i].upload_ctx.recursive = ctx->deploy.jfrog[i].upload_ctx.recursive;
        result->deploy.jfrog[i].upload_ctx.regexp = ctx->deploy.jfrog[i].upload_ctx.regexp;
        result->deploy.jfrog[i].upload_ctx.retries = ctx->deploy.jfrog[i].upload_ctx.retries;
        result->deploy.jfrog[i].upload_ctx.retry_wait_time = ctx->deploy.jfrog[i].upload_ctx.retry_wait_time;
        result->deploy.jfrog[i].upload_ctx.spec = strdup_maybe(ctx->deploy.jfrog[i].upload_ctx.spec);
        result->deploy.jfrog[i].upload_ctx.spec_vars = strdup_maybe(ctx->deploy.jfrog[i].upload_ctx.spec_vars);
        result->deploy.jfrog[i].upload_ctx.symlinks = ctx->deploy.jfrog[i].upload_ctx.symlinks;
        result->deploy.jfrog[i].upload_ctx.sync_deletes = ctx->deploy.jfrog[i].upload_ctx.sync_deletes;
        result->deploy.jfrog[i].upload_ctx.target_props = strdup_maybe(ctx->deploy.jfrog[i].upload_ctx.target_props);
        result->deploy.jfrog[i].upload_ctx.threads = ctx->deploy.jfrog[i].upload_ctx.threads;
        result->deploy.jfrog[i].upload_ctx.workaround_parent_only = ctx->deploy.jfrog[i].upload_ctx.workaround_parent_only;
    }

    return result;
}

void delivery_free(struct Delivery *ctx) {
    guard_free(ctx->system.arch);
    guard_array_free(ctx->system.platform);
    guard_free(ctx->meta.name);
    guard_free(ctx->meta.version);
    guard_free(ctx->meta.codename);
    guard_free(ctx->meta.mission);
    guard_free(ctx->meta.python);
    guard_free(ctx->meta.mission);
    guard_free(ctx->meta.python_compact);
    guard_free(ctx->meta.based_on);
    guard_runtime_free(ctx->runtime.environ);
    guard_free(ctx->storage.root);
    guard_free(ctx->storage.tmpdir);
    guard_free(ctx->storage.delivery_dir);
    guard_free(ctx->storage.tools_dir);
    guard_free(ctx->storage.package_dir);
    guard_free(ctx->storage.results_dir);
    guard_free(ctx->storage.output_dir);
    guard_free(ctx->storage.conda_install_prefix);
    guard_free(ctx->storage.conda_artifact_dir);
    guard_free(ctx->storage.conda_staging_dir);
    guard_free(ctx->storage.conda_staging_url);
    guard_free(ctx->storage.wheel_artifact_dir);
    guard_free(ctx->storage.wheel_staging_dir);
    guard_free(ctx->storage.wheel_staging_url);
    guard_free(ctx->storage.build_dir);
    guard_free(ctx->storage.build_recipes_dir);
    guard_free(ctx->storage.build_sources_dir);
    guard_free(ctx->storage.build_testing_dir);
    guard_free(ctx->storage.build_docker_dir);
    guard_free(ctx->storage.mission_dir);
    guard_free(ctx->storage.docker_artifact_dir);
    guard_free(ctx->storage.meta_dir);
    guard_free(ctx->storage.package_dir);
    guard_free(ctx->storage.cfgdump_dir);
    guard_free(ctx->info.time_str_epoch);
    guard_free(ctx->info.build_name);
    guard_free(ctx->info.build_number);
    guard_free(ctx->info.release_name);
    guard_free(ctx->info.time_info);
    guard_free(ctx->conda.installer_baseurl);
    guard_free(ctx->conda.installer_name);
    guard_free(ctx->conda.installer_version);
    guard_free(ctx->conda.installer_platform);
    guard_free(ctx->conda.installer_arch);
    guard_free(ctx->conda.installer_path);
    guard_free(ctx->conda.tool_version);
    guard_free(ctx->conda.tool_build_version);
    guard_strlist_free(&ctx->conda.conda_packages);
    guard_strlist_free(&ctx->conda.conda_packages_defer);
    guard_strlist_free(&ctx->conda.conda_packages_purge);
    guard_strlist_free(&ctx->conda.pip_packages);
    guard_strlist_free(&ctx->conda.pip_packages_defer);
    guard_strlist_free(&ctx->conda.pip_packages_purge);
    guard_strlist_free(&ctx->conda.wheels_packages);

    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        guard_free(ctx->tests[i].name);
        guard_free(ctx->tests[i].version);
        guard_free(ctx->tests[i].repository);
        guard_free(ctx->tests[i].repository_info_ref);
        guard_free(ctx->tests[i].repository_info_tag);
        guard_strlist_free(&ctx->tests[i].repository_remove_tags);
        guard_free(ctx->tests[i].script);
        guard_free(ctx->tests[i].script_setup);
        guard_free(ctx->tests[i].build_recipe);
        // test-specific runtime variables
        guard_runtime_free(ctx->tests[i].runtime.environ);
    }

    guard_free(ctx->rules.release_fmt);
    guard_free(ctx->rules.build_name_fmt);
    guard_free(ctx->rules.build_number_fmt);

    guard_free(ctx->deploy.docker.test_script);
    guard_free(ctx->deploy.docker.registry);
    guard_free(ctx->deploy.docker.image_compression);
    guard_strlist_free(&ctx->deploy.docker.tags);
    guard_strlist_free(&ctx->deploy.docker.build_args);

    for (size_t i = 0; i < sizeof(ctx->deploy.jfrog) / sizeof(ctx->deploy.jfrog[0]); i++) {
        guard_free(ctx->deploy.jfrog[i].repo);
        guard_free(ctx->deploy.jfrog[i].dest);
        guard_strlist_free(&ctx->deploy.jfrog[i].files);
    }

    if (ctx->_stasis_ini_fp.delivery) {
        ini_free(&ctx->_stasis_ini_fp.delivery);
    }
    guard_free(ctx->_stasis_ini_fp.delivery_path);

    if (ctx->_stasis_ini_fp.cfg) {
        // optional extras
        ini_free(&ctx->_stasis_ini_fp.cfg);
    }
    guard_free(ctx->_stasis_ini_fp.cfg_path);

    if (ctx->_stasis_ini_fp.mission) {
        ini_free(&ctx->_stasis_ini_fp.mission);
    }
    guard_free(ctx->_stasis_ini_fp.mission_path);
}

int delivery_format_str(struct Delivery *ctx, char **dest, const char *fmt) {
    size_t fmt_len = strlen(fmt);

    if (!*dest) {
        *dest = calloc(STASIS_NAME_MAX, sizeof(**dest));
        if (!*dest) {
            return -1;
        }
    }

    for (size_t i = 0; i < fmt_len; i++) {
        if (fmt[i] == '%' && strlen(&fmt[i])) {
            i++;
            switch (fmt[i]) {
                case 'n':   // name
                    strcat(*dest, ctx->meta.name);
                    break;
                case 'c':   // codename
                    strcat(*dest, ctx->meta.codename);
                    break;
                case 'm':   // mission
                    strcat(*dest, ctx->meta.mission);
                    break;
                case 'r':   // revision
                    sprintf(*dest + strlen(*dest), "%d", ctx->meta.rc);
                    break;
                case 'R':   // "final"-aware revision
                    if (ctx->meta.final)
                        strcat(*dest, "final");
                    else
                        sprintf(*dest + strlen(*dest), "%d", ctx->meta.rc);
                    break;
                case 'v':   // version
                    strcat(*dest, ctx->meta.version);
                    break;
                case 'P':   // python version
                    strcat(*dest, ctx->meta.python);
                    break;
                case 'p':   // python version major/minor
                    strcat(*dest, ctx->meta.python_compact);
                    break;
                case 'a':   // system architecture name
                    strcat(*dest, ctx->system.arch);
                    break;
                case 'o':   // system platform (OS) name
                    strcat(*dest, ctx->system.platform[DELIVERY_PLATFORM_RELEASE]);
                    break;
                case 't':   // unix epoch
                    sprintf(*dest + strlen(*dest), "%ld", ctx->info.time_now);
                    break;
                default:    // unknown formatter, write as-is
                    sprintf(*dest + strlen(*dest), "%c%c", fmt[i - 1], fmt[i]);
                    break;
            }
        } else {    // write non-format text
            sprintf(*dest + strlen(*dest), "%c", fmt[i]);
        }
    }
    return 0;
}

void delivery_defer_packages(struct Delivery *ctx, int type) {
    struct StrList *dataptr = NULL;
    struct StrList *deferred = NULL;
    char *name = NULL;

    char mode[10];
    if (DEFER_CONDA == type) {
        dataptr = ctx->conda.conda_packages;
        deferred = ctx->conda.conda_packages_defer;
        strcpy(mode, "conda");
    } else if (DEFER_PIP == type) {
        dataptr = ctx->conda.pip_packages;
        deferred = ctx->conda.pip_packages_defer;
        strcpy(mode, "pip");
    } else {
        SYSERROR("BUG: type %d does not map to a supported package manager!\n", type);
        exit(1);
    }
    msg(STASIS_MSG_L2, "Filtering %s packages by test definition...\n", mode);

    struct StrList *filtered = NULL;
    filtered = strlist_init();
    for (size_t i = 0; i < strlist_count(dataptr); i++) {
        int build_for_host = 0;

        name = strlist_item(dataptr, i);
        if (!strlen(name) || isblank(*name) || isspace(*name)) {
            // no data
            continue;
        }

        // Compile a list of packages that are *also* to be tested.
        char *spec_begin = strpbrk(name, "@~=<>!");
        char *spec_end = spec_begin;
        char package_name[255] = {0};

        if (spec_end) {
            // A version is present in the package name. Jump past operator(s).
            while (*spec_end != '\0' && !isalnum(*spec_end)) {
                spec_end++;
            }
            strncpy(package_name, name, spec_begin - name);
        } else {
            strncpy(package_name, name, sizeof(package_name) - 1);
        }
        remove_extras(package_name);

        msg(STASIS_MSG_L3, "package '%s': ", package_name);

        // When spec is present in name, set tests->version to the version detected in the name
        for (size_t x = 0; x < sizeof(ctx->tests) / sizeof(ctx->tests[0]) && ctx->tests[x].name != NULL; x++) {
            struct Test *test = &ctx->tests[x];
            char nametmp[1024] = {0};

            strncpy(nametmp, package_name, sizeof(nametmp) - 1);
            // Is the [test:NAME] in the package name?
            if (!strcmp(nametmp, test->name)) {
                // Override test->version when a version is provided by the (pip|conda)_package list item
                guard_free(test->version);
                if (spec_begin && spec_end) {
                    char *version_at = strrchr(spec_end, '@');
                    if (version_at) {
                        if (strlen(version_at)) {
                            version_at++;
                        }
                        test->version = strdup(version_at);
                    } else {
                        test->version = strdup(spec_end);
                    }
                } else {
                    // There are too many possible default branches nowadays: master, main, develop, xyz, etc.
                    // HEAD is a safe bet.
                    test->version = strdup("HEAD");
                }

                // Is the list item a git+schema:// URL?
                // TODO: nametmp is just the name so this will never work. but do we want it to? this looks like
                // TODO:     an unsafe feature. We shouldn't be able to change what's in the config. we should
                // TODO:     be getting what we asked for, or exit the program with an error.
                if (strstr(nametmp, "git+") && strstr(nametmp, "://")) {
                    char *xrepo = strstr(nametmp, "+");
                    if (xrepo) {
                        xrepo++;
                        guard_free(test->repository);
                        test->repository = strdup(xrepo);
                        xrepo = NULL;
                    }
                    // Extract the name of the package
                    char *xbasename = path_basename(nametmp);
                    if (xbasename) {
                        // Replace the git+schema:// URL with the package name
                        strlist_set(&dataptr, i, xbasename);
                        name = strlist_item(dataptr, i);
                    }
                }

                int upstream_exists = 0;
                if (DEFER_PIP == type) {
                    upstream_exists = pkg_index_provides(PKG_USE_PIP, PYPI_INDEX_DEFAULT, name);
                } else if (DEFER_CONDA == type) {
                    upstream_exists = pkg_index_provides(PKG_USE_CONDA, NULL, name);
                }

                if (PKG_INDEX_PROVIDES_FAILED(upstream_exists)) {
                    fprintf(stderr, "%s's existence command failed for '%s': %s\n",
                            mode, name, pkg_index_provides_strerror(upstream_exists));
                    exit(1);
                }

                if (upstream_exists == PKG_NOT_FOUND) {
                    build_for_host = 1;
                } else {
                    build_for_host = 0;
                }

                break;
            }
        }

        if (build_for_host) {
            printf("BUILD FOR HOST\n");
            strlist_append(&deferred, name);
        } else {
            printf("USE EXTERNAL\n");
            strlist_append(&filtered, name);
        }
    }

    if (!strlist_count(deferred)) {
        msg(STASIS_MSG_WARN | STASIS_MSG_L2, "No %s packages were filtered by test definitions\n", mode);
    } else {
        if (DEFER_CONDA == type) {
            guard_strlist_free(&ctx->conda.conda_packages);
            ctx->conda.conda_packages = strlist_copy(filtered);
        } else if (DEFER_PIP == type) {
            guard_strlist_free(&ctx->conda.pip_packages);
            ctx->conda.pip_packages = strlist_copy(filtered);
        }
    }
    if (filtered) {
        strlist_free(&filtered);
    }
}

int delivery_gather_tool_versions(struct Delivery *ctx) {
    int status_tool_version = 0;
    int status_tool_build_version = 0;

    // Extract version from tool output
    ctx->conda.tool_version = shell_output("conda --version", &status_tool_version);
    if (ctx->conda.tool_version)
        strip(ctx->conda.tool_version);

    ctx->conda.tool_build_version = shell_output("conda build --version", &status_tool_build_version);
    if (ctx->conda.tool_build_version)
        strip(ctx->conda.tool_version);

    if (status_tool_version || status_tool_build_version) {
        return 1;
    }
    return 0;
}

