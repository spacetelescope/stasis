#include <getopt.h>
#include <fnmatch.h>
#include "core.h"

static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"destdir", required_argument, 0, 'd'},
        {"verbose", no_argument, 0, 'v'},
        {"unbuffered", no_argument, 0, 'U'},
        {"web", no_argument, 0, 'w'},
        {0, 0, 0, 0},
};

const char *long_options_help[] = {
        "Display this usage statement",
        "Destination directory",
        "Increase output verbosity",
        "Disable line buffering",
        "Generate HTML indexes (requires pandoc)",
        NULL,
};

static void usage(char *name) {
    int maxopts = sizeof(long_options) / sizeof(long_options[0]);
    unsigned char *opts = calloc(maxopts + 1, sizeof(char));
    for (int i = 0; i < maxopts; i++) {
        opts[i] = long_options[i].val;
    }
    printf("usage: %s [-%s] {{STASIS_ROOT}...}\n", name, opts);
    guard_free(opts);

    for (int i = 0; i < maxopts - 1; i++) {
        char line[255];
        sprintf(line, "  --%s  -%c  %-20s", long_options[i].name, long_options[i].val, long_options_help[i]);
        puts(line);
    }

}

int indexer_combine_rootdirs(const char *dest, char **rootdirs, const size_t rootdirs_total) {
    char cmd[PATH_MAX];

    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "rsync -ah%s --delete --exclude 'tools/' --exclude 'tmp/' --exclude 'build/' ", globals.verbose ? "v" : "q");
    for (size_t i = 0; i < rootdirs_total; i++) {
        sprintf(cmd + strlen(cmd), "'%s'/ ", rootdirs[i]);
    }
    sprintf(cmd + strlen(cmd), "%s/", dest);

    if (globals.verbose) {
        puts(cmd);
    }

    if (system(cmd)) {
        return -1;
    }
    return 0;
}

int indexer_wheels(struct Delivery *ctx) {
    return delivery_index_wheel_artifacts(ctx);
}

int indexer_load_metadata(struct Delivery *ctx, const char *filename) {
    char line[STASIS_NAME_MAX] = {0};
    FILE *fp;

    fp = fopen(filename, "r");
    if (!fp) {
        return -1;
    }

    while (fgets(line, sizeof(line) - 1, fp) != NULL) {
        char **parts = split(line, " ", 1);
        char *name = parts[0];
        char *value = parts[1];
        strip(value);
        if (!strcmp(name, "name")) {
            ctx->meta.name = strdup(value);
        } else if (!strcmp(name, "version")) {
            ctx->meta.version = strdup(value);
        } else if (!strcmp(name, "rc")) {
            ctx->meta.rc = (int) strtol(value, NULL, 10);
        } else if (!strcmp(name, "python")) {
            ctx->meta.python = strdup(value);
        } else if (!strcmp(name, "python_compact")) {
            ctx->meta.python_compact = strdup(value);
        } else if (!strcmp(name, "mission")) {
            ctx->meta.mission = strdup(value);
        } else if (!strcmp(name, "codename")) {
            ctx->meta.codename = strdup(value);
        } else if (!strcmp(name, "platform")) {
            ctx->system.platform = calloc(DELIVERY_PLATFORM_MAX, sizeof(*ctx->system.platform));
            char **platform = split(value, " ", 0);
            ctx->system.platform[DELIVERY_PLATFORM] = platform[DELIVERY_PLATFORM];
            ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR] = platform[DELIVERY_PLATFORM_CONDA_SUBDIR];
            ctx->system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER] = platform[DELIVERY_PLATFORM_CONDA_INSTALLER];
            ctx->system.platform[DELIVERY_PLATFORM_RELEASE] = platform[DELIVERY_PLATFORM_RELEASE];
        } else if (!strcmp(name, "arch")) {
            ctx->system.arch = strdup(value);
        } else if (!strcmp(name, "time")) {
            ctx->info.time_str_epoch = strdup(value);
        } else if (!strcmp(name, "release_fmt")) {
            ctx->rules.release_fmt = strdup(value);
        } else if (!strcmp(name, "release_name")) {
            ctx->info.release_name = strdup(value);
        } else if (!strcmp(name, "build_name_fmt")) {
            ctx->rules.build_name_fmt = strdup(value);
        } else if (!strcmp(name, "build_name")) {
            ctx->info.build_name = strdup(value);
        } else if (!strcmp(name, "build_number_fmt")) {
            ctx->rules.build_number_fmt = strdup(value);
        } else if (!strcmp(name, "build_number")) {
            ctx->info.build_number = strdup(value);
        } else if (!strcmp(name, "conda_installer_baseurl")) {
            ctx->conda.installer_baseurl = strdup(value);
        } else if (!strcmp(name, "conda_installer_name")) {
            ctx->conda.installer_name = strdup(value);
        } else if (!strcmp(name, "conda_installer_version")) {
            ctx->conda.installer_version = strdup(value);
        } else if (!strcmp(name, "conda_installer_platform")) {
            ctx->conda.installer_platform = strdup(value);
        } else if (!strcmp(name, "conda_installer_arch")) {
            ctx->conda.installer_arch = strdup(value);
        }
        GENERIC_ARRAY_FREE(parts);
    }
    fclose(fp);

    return 0;
}

int indexer_get_files(struct StrList **out, const char *path, const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    char userpattern[PATH_MAX] = {0};
    vsprintf(userpattern, pattern, args);
    va_end(args);
    struct StrList *list = listdir(path);
    if (!list) {
        return -1;
    }

    if (!(*out)) {
        (*out) = strlist_init();
        if (!(*out)) {
            guard_strlist_free(&list);
            return -1;
        }
    }

    size_t no_match = 0;
    for (size_t i = 0; i < strlist_count(list); i++) {
        char *item = strlist_item(list, i);
        if (fnmatch(userpattern, item, 0)) {
            no_match++;
            continue;
        } else {
            strlist_append(&(*out), item);
        }
    }
    if (no_match >= strlist_count(list)) {
        fprintf(stderr, "no files matching the pattern: %s\n", userpattern);
        guard_strlist_free(&list);
        return -1;
    }
    guard_strlist_free(&list);
    return 0;
}

int get_latest_rc(struct Delivery ctx[], size_t nelem) {
    int result = 0;
    for (size_t i = 0; i < nelem; i++) {
        if (ctx[i].meta.rc > result) {
            result = ctx[i].meta.rc;
        }
    }
    return result;
}

struct Delivery **get_latest_deliveries(struct Delivery ctx[], size_t nelem) {
    struct Delivery **result = NULL;
    int latest = 0;
    size_t n = 0;

    result = calloc(nelem + 1, sizeof(result));
    if (!result) {
        fprintf(stderr, "Unable to allocate %zu bytes for result delivery array: %s\n", nelem * sizeof(result), strerror(errno));
        return NULL;
    }

    latest = get_latest_rc(ctx, nelem);
    for (size_t i = 0; i < nelem; i++) {
        if (ctx[i].meta.rc == latest) {
            result[n] = &ctx[i];
            n++;
        }
    }

    return result;
}

int indexer_make_website(struct Delivery *ctx) {
    char cmd[PATH_MAX];
    char *inputs[] = {
        ctx->storage.delivery_dir, "/README.md",
        ctx->storage.results_dir, "/README.md",
    };

    if (!find_program("pandoc")) {
        fprintf(stderr, "pandoc is not installed: unable to generate HTML indexes\n");
        return 0;
    }

    for (size_t i = 0; i < sizeof(inputs) / sizeof(*inputs); i += 2) {
        char fullpath[PATH_MAX];
        memset(fullpath, 0, sizeof(fullpath));
        sprintf(fullpath, "%s/%s", inputs[i], inputs[i + 1]);
        if (access(fullpath, F_OK)) {
            continue;
        }
        // Converts a markdown file to html
        strcpy(cmd, "pandoc ");
        strcat(cmd, fullpath);
        strcat(cmd, " ");
        strcat(cmd, "-o ");
        strcat(cmd, inputs[i]);
        strcat(cmd, "/index.html");
        if (globals.verbose) {
            puts(cmd);
        }
        // This might be negative when killed by a signal.
        // Otherwise, the return code is not critical to us.
        if (system(cmd) < 0) {
            return 1;
        }
    }

    return 0;
}

int indexer_conda(struct Delivery *ctx) {
    int status = 0;
    char micromamba_prefix[PATH_MAX] = {0};
    sprintf(micromamba_prefix, "%s/bin", ctx->storage.tools_dir);
    struct MicromambaInfo m = {.conda_prefix = globals.conda_install_prefix, .micromamba_prefix = micromamba_prefix};

    status += micromamba(&m, "config prepend --env channels conda-forge");
    if (!globals.verbose) {
        status += micromamba(&m, "config set --env quiet true");
    }
    status += micromamba(&m, "config set --env always_yes true");
    status += micromamba(&m, "install conda-build");
    status += micromamba(&m, "run conda index %s", ctx->storage.conda_artifact_dir);
    return status;
}

static struct StrList *get_architectures(struct Delivery ctx[], size_t nelem) {
    struct StrList *architectures = strlist_init();
    for (size_t i = 0; i < nelem; i++) {
        if (!strstr_array(architectures->data, ctx[i].system.arch)) {
            strlist_append(&architectures, ctx[i].system.arch);
        }
    }
    return architectures;
}

static struct StrList *get_platforms(struct Delivery ctx[], size_t nelem) {
    struct StrList *platforms = strlist_init();
    for (size_t i = 0; i < nelem; i++) {
        if (!strstr_array(platforms->data, ctx[i].system.platform[DELIVERY_PLATFORM_RELEASE])) {
            strlist_append(&platforms, ctx[i].system.platform[DELIVERY_PLATFORM_RELEASE]);
        }
    }
    return platforms;
}

int indexer_symlinks(struct Delivery ctx[], size_t nelem) {
    struct Delivery **data = NULL;
    data = get_latest_deliveries(ctx, nelem);
    //int latest = get_latest_rc(ctx, nelem);

    if (!pushd(ctx->storage.delivery_dir)) {
        for (size_t i = 0; i < nelem; i++) {
            char link_name_spec[PATH_MAX];
            char link_name_readme[PATH_MAX];

            char file_name_spec[PATH_MAX];
            char file_name_readme[PATH_MAX];

            if (!data[i]) {
                continue;
            }
            sprintf(link_name_spec, "latest-py%s-%s-%s.yml", data[i]->meta.python_compact, data[i]->system.platform[DELIVERY_PLATFORM_RELEASE], data[i]->system.arch);
            sprintf(file_name_spec, "%s.yml", data[i]->info.release_name);

            sprintf(link_name_readme, "README-py%s-%s-%s.md", data[i]->meta.python_compact, data[i]->system.platform[DELIVERY_PLATFORM_RELEASE], data[i]->system.arch);
            sprintf(file_name_readme, "README-%s.md", data[i]->info.release_name);

            if (!access(link_name_spec, F_OK)) {
                if (unlink(link_name_spec)) {
                    fprintf(stderr, "Unable to remove spec link: %s\n", link_name_spec);
                }
            }
            if (!access(link_name_readme, F_OK)) {
                if (unlink(link_name_readme)) {
                    fprintf(stderr, "Unable to remove readme link: %s\n", link_name_readme);
                }
            }

            if (globals.verbose) {
                printf("%s -> %s\n", file_name_spec, link_name_spec);
            }
            if (symlink(file_name_spec, link_name_spec)) {
                fprintf(stderr, "Unable to link %s as %s\n", file_name_spec, link_name_spec);
            }

            if (globals.verbose) {
                printf("%s -> %s\n", file_name_readme, link_name_readme);
            }
            if (symlink(file_name_readme, link_name_readme)) {
                fprintf(stderr, "Unable to link %s as %s\n", file_name_readme, link_name_readme);
            }
        }
        popd();
    } else {
        fprintf(stderr, "Unable to enter delivery directory: %s\n", ctx->storage.delivery_dir);
        guard_free(data);
        return -1;
    }

    // "latest" is an array of pointers to ctx[]. Do not free the contents of the array.
    guard_free(data);
    return 0;
}

int indexer_readmes(struct Delivery ctx[], size_t nelem) {
    struct Delivery **latest = NULL;
    latest = get_latest_deliveries(ctx, nelem);

    char indexfile[PATH_MAX] = {0};
    sprintf(indexfile, "%s/README.md", ctx->storage.delivery_dir);

    if (!pushd(ctx->storage.delivery_dir)) {
        FILE *indexfp;
        indexfp = fopen(indexfile, "w+");
        if (!indexfp) {
            fprintf(stderr, "Unable to open %s for writing\n", indexfile);
            return -1;
        }
        struct StrList *archs = get_architectures(*latest, nelem);
        struct StrList *platforms = get_platforms(*latest, nelem);

        fprintf(indexfp, "# %s-%s\n\n", ctx->meta.name, ctx->meta.version);
        fprintf(indexfp, "## Current Release\n\n");
        for (size_t p = 0; p < strlist_count(platforms); p++) {
            char *platform = strlist_item(platforms, p);
            for (size_t a = 0; a < strlist_count(archs); a++) {
                char *arch = strlist_item(archs, a);
                int have_combo = 0;
                for (size_t i = 0; i < nelem; i++) {
                    if (strstr(latest[i]->system.platform[DELIVERY_PLATFORM_RELEASE], platform) && strstr(latest[i]->system.arch, arch)) {
                        have_combo = 1;
                    }
                }
                if (!have_combo) {
                    continue;
                }
                fprintf(indexfp, "### %s-%s\n\n", platform, arch);

                fprintf(indexfp, "|Release|Info|Receipt|\n");
                fprintf(indexfp, "|:----:|:----:|:----:|\n");
                for (size_t i = 0; i < nelem; i++) {
                    char link_name[PATH_MAX];
                    char readme_name[PATH_MAX];
                    char conf_name[PATH_MAX];
                    char conf_name_relative[PATH_MAX];
                    if (!latest[i]) {
                        continue;
                    }
                    sprintf(link_name, "latest-py%s-%s-%s.yml", latest[i]->meta.python_compact, latest[i]->system.platform[DELIVERY_PLATFORM_RELEASE], latest[i]->system.arch);
                    sprintf(readme_name, "README-py%s-%s-%s.md", latest[i]->meta.python_compact, latest[i]->system.platform[DELIVERY_PLATFORM_RELEASE], latest[i]->system.arch);
                    sprintf(conf_name, "%s.ini", latest[i]->info.release_name);
                    sprintf(conf_name_relative, "../config/%s-rendered.ini", latest[i]->info.release_name);
                    if (strstr(link_name, platform) && strstr(link_name, arch)) {
                        fprintf(indexfp, "|[%s](%s)|[%s](%s)|[%s](%s)|\n", link_name, link_name, readme_name, readme_name, conf_name, conf_name_relative);
                    }
                }
                fprintf(indexfp, "\n");
            }
            fprintf(indexfp, "\n");
        }
        guard_strlist_free(&archs);
        guard_strlist_free(&platforms);
        fclose(indexfp);
        popd();
    } else {
        fprintf(stderr, "Unable to enter delivery directory: %s\n", ctx->storage.delivery_dir);
        guard_free(latest);
        return -1;
    }

    // "latest" is an array of pointers to ctxs[]. Do not free the contents of the array.
    guard_free(latest);
    return 0;
}

int indexer_junitxml_report(struct Delivery ctx[], size_t nelem) {
    struct Delivery **latest = NULL;
    latest = get_latest_deliveries(ctx, nelem);

    char indexfile[PATH_MAX] = {0};
    sprintf(indexfile, "%s/README.md", ctx->storage.results_dir);

    struct StrList *file_listing = listdir(ctx->storage.results_dir);
    if (!file_listing) {
        // no test results to process
        return 0;
    }

    if (!pushd(ctx->storage.results_dir)) {
        FILE *indexfp;
        indexfp = fopen(indexfile, "w+");
        if (!indexfp) {
            fprintf(stderr, "Unable to open %s for writing\n", indexfile);
            return -1;
        }
        struct StrList *archs = get_architectures(*latest, nelem);
        struct StrList *platforms = get_platforms(*latest, nelem);

        fprintf(indexfp, "# %s-%s Test Report\n\n", ctx->meta.name, ctx->meta.version);
        fprintf(indexfp, "## Current Release\n\n");
        for (size_t p = 0; p < strlist_count(platforms); p++) {
            char *platform = strlist_item(platforms, p);
            for (size_t a = 0; a < strlist_count(archs); a++) {
                char *arch = strlist_item(archs, a);
                int have_combo = 0;
                for (size_t i = 0; i < nelem; i++) {
                    if (strstr(latest[i]->system.platform[DELIVERY_PLATFORM_RELEASE], platform) && strstr(latest[i]->system.arch, arch)) {
                        have_combo = 1;
                        break;
                    }
                }
                if (!have_combo) {
                    continue;
                }
                fprintf(indexfp, "### %s-%s\n\n", platform, arch);

                fprintf(indexfp, "|Suite|Duration|Fail    |Skip |Error |\n");
                fprintf(indexfp, "|:----|:------:|:------:|:---:|:----:|\n");
                for (size_t f = 0; f < strlist_count(file_listing); f++) {
                    char *filename = strlist_item(file_listing, f);
                    if (!endswith(filename, ".xml")) {
                        continue;
                    }

                    if (strstr(filename, platform) && strstr(filename, arch)) {
                        struct JUNIT_Testsuite *testsuite = junitxml_testsuite_read(filename);
                        if (testsuite) {
                            if (globals.verbose) {
                                printf("%s: duration: %0.4f, failed: %d, skipped: %d, errors: %d\n", filename, testsuite->time, testsuite->failures, testsuite->skipped, testsuite->errors);
                            }
                            fprintf(indexfp, "|[%s](%s)|%0.4f|%d|%d|%d|\n", filename, filename, testsuite->time, testsuite->failures, testsuite->skipped, testsuite->errors);
                            /*
                             * TODO: Display failure/skip/error output.
                             *
                            for (size_t i = 0; i < testsuite->_tc_inuse; i++) {
                                if (testsuite->testcase[i]->tc_result_state_type) {
                                    printf("testcase: %s :: %s\n", testsuite->testcase[i]->classname, testsuite->testcase[i]->name);
                                    if (testsuite->testcase[i]->tc_result_state_type == JUNIT_RESULT_STATE_FAILURE) {
                                        printf("failure: %s\n", testsuite->testcase[i]->result_state.failure->message);
                                    } else if (testsuite->testcase[i]->tc_result_state_type == JUNIT_RESULT_STATE_SKIPPED) {
                                        printf("skipped: %s\n", testsuite->testcase[i]->result_state.skipped->message);
                                    }
                                }
                            }
                             */
                            junitxml_testsuite_free(&testsuite);
                        } else {
                            fprintf(stderr, "bad test suite: %s: %s\n", strerror(errno), filename);
                            continue;
                        }
                    }
                }
                fprintf(indexfp, "\n");
            }
            fprintf(indexfp, "\n");
        }
        guard_strlist_free(&archs);
        guard_strlist_free(&platforms);
        fclose(indexfp);
        popd();
    } else {
        fprintf(stderr, "Unable to enter delivery directory: %s\n", ctx->storage.delivery_dir);
        guard_free(latest);
        return -1;
    }

    // "latest" is an array of pointers to ctxs[]. Do not free the contents of the array.
    guard_free(latest);
    return 0;
}

void indexer_init_dirs(struct Delivery *ctx, const char *workdir) {
    path_store(&ctx->storage.root, PATH_MAX, workdir, "");
    path_store(&ctx->storage.tmpdir, PATH_MAX, ctx->storage.root, "tmp");
    if (delivery_init_tmpdir(ctx)) {
        fprintf(stderr, "Failed to configure temporary storage directory\n");
        exit(1);
    }
    path_store(&ctx->storage.output_dir, PATH_MAX, ctx->storage.root, "output");
    path_store(&ctx->storage.tools_dir, PATH_MAX, ctx->storage.output_dir, "tools");
    path_store(&globals.conda_install_prefix, PATH_MAX, ctx->storage.tools_dir, "conda");
    path_store(&ctx->storage.cfgdump_dir, PATH_MAX, ctx->storage.output_dir, "config");
    path_store(&ctx->storage.meta_dir, PATH_MAX, ctx->storage.output_dir, "meta");
    path_store(&ctx->storage.delivery_dir, PATH_MAX, ctx->storage.output_dir, "delivery");
    path_store(&ctx->storage.package_dir, PATH_MAX, ctx->storage.output_dir, "packages");
    path_store(&ctx->storage.results_dir, PATH_MAX, ctx->storage.output_dir, "results");
    path_store(&ctx->storage.wheel_artifact_dir, PATH_MAX, ctx->storage.package_dir, "wheels");
    path_store(&ctx->storage.conda_artifact_dir, PATH_MAX, ctx->storage.package_dir, "conda");

    char newpath[PATH_MAX] = {0};
    if (getenv("PATH")) {
        sprintf(newpath, "%s/bin:%s", ctx->storage.tools_dir, getenv("PATH"));
        setenv("PATH", newpath, 1);
    } else {
        SYSERROR("%s", "environment variable PATH is undefined. Unable to continue.");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    size_t rootdirs_total = 0;
    char *destdir = NULL;
    char **rootdirs = NULL;
    int do_html = 0;
    int c = 0;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "hd:vUw", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                usage(path_basename(argv[0]));
                exit(0);
            case 'd':
                destdir = strdup(optarg);
                break;
            case 'U':
                fflush(stdout);
                fflush(stderr);
                setvbuf(stdout, NULL, _IONBF, 0);
                setvbuf(stderr, NULL, _IONBF, 0);
                break;
            case 'v':
                globals.verbose = 1;
                break;
            case 'w':
                do_html = 1;
                break;
            case '?':
            default:
                exit(1);
        }
    }

    int current_index = optind;
    if (optind < argc) {
        rootdirs_total = argc - current_index;
        while (optind < argc) {
            // use first positional argument
            rootdirs = &argv[optind++];
            break;
        }
    }

    if (isempty(destdir)) {
        destdir = strdup("output");
    }

    if (!rootdirs || !rootdirs_total) {
        fprintf(stderr, "You must specify at least one STASIS root directory to index\n");
        exit(1);
    } else {
        for (size_t i = 0; i < rootdirs_total; i++) {
            if (isempty(rootdirs[i]) || !strcmp(rootdirs[i], "/") || !strcmp(rootdirs[i], "\\")) {
                SYSERROR("Unsafe directory: %s", rootdirs[i]);
                exit(1);
            }
        }
    }

    char *workdir;
    char workdir_template[PATH_MAX] = {0};
    char *system_tmp = getenv("TMPDIR");
    if (system_tmp) {
        strcat(workdir_template, system_tmp);
    } else {
        strcat(workdir_template, "/tmp");
    }
    strcat(workdir_template, "/stasis-combine.XXXXXX");
    workdir = mkdtemp(workdir_template);
    if (!workdir) {
        SYSERROR("Unable to create temporary directory: %s", workdir_template);
        exit(1);
    } else if (isempty(workdir) || !strcmp(workdir, "/") || !strcmp(workdir, "\\")) {
        SYSERROR("Unsafe directory: %s", workdir);
        exit(1);
    }

    struct Delivery ctx;
    memset(&ctx, 0, sizeof(ctx));

    printf(BANNER, VERSION, AUTHOR);

    indexer_init_dirs(&ctx, workdir);

    msg(STASIS_MSG_L1, "%s delivery root %s\n",
        rootdirs_total > 1 ? "Merging" : "Indexing",
        rootdirs_total > 1 ? "directories" : "directory");
    if (indexer_combine_rootdirs(workdir, rootdirs, rootdirs_total)) {
        SYSERROR("%s", "Copy operation failed");
        rmtree(workdir);
        exit(1);
    }

    if (access(ctx.storage.conda_artifact_dir, F_OK)) {
        mkdirs(ctx.storage.conda_artifact_dir, 0755);
    }

    if (access(ctx.storage.wheel_artifact_dir, F_OK)) {
        mkdirs(ctx.storage.wheel_artifact_dir, 0755);
    }

    msg(STASIS_MSG_L1, "Indexing conda packages\n");
    if (indexer_conda(&ctx)) {
        SYSERROR("%s", "Conda package indexing operation failed");
        exit(1);
    }

    msg(STASIS_MSG_L1, "Indexing wheel packages\n");
    if (indexer_wheels(&ctx)) {
        SYSERROR("%s", "Python package indexing operation failed");
        exit(1);
    }

    msg(STASIS_MSG_L1, "Loading metadata\n");
    struct StrList *metafiles = NULL;
    indexer_get_files(&metafiles, ctx.storage.meta_dir, "*.stasis");
    strlist_sort(metafiles, STASIS_SORT_LEN_ASCENDING);
    struct Delivery local[strlist_count(metafiles)];

    for (size_t i = 0; i < strlist_count(metafiles); i++) {
        char *item = strlist_item(metafiles, i);
        memset(&local[i], 0, sizeof(ctx));
        memcpy(&local[i], &ctx, sizeof(ctx));
        char path[PATH_MAX];
        sprintf(path, "%s/%s", ctx.storage.meta_dir, item);
        if (globals.verbose) {
            puts(path);
        }
        indexer_load_metadata(&local[i], path);
    }

    msg(STASIS_MSG_L1, "Generating links to latest release iteration\n");
    if (indexer_symlinks(local, strlist_count(metafiles))) {
        SYSERROR("%s", "Link generation failed");
        exit(1);
    }

    msg(STASIS_MSG_L1, "Generating README.md\n");
    if (indexer_readmes(local, strlist_count(metafiles))) {
        SYSERROR("%s", "README indexing operation failed");
        exit(1);
    }

    msg(STASIS_MSG_L1, "Indexing test results\n");
    if (indexer_junitxml_report(local, strlist_count(metafiles))) {
        SYSERROR("%s", "Test result indexing operation failed");
        exit(1);
    }

    if (do_html) {
        msg(STASIS_MSG_L1, "Generating HTML indexes\n");
        if (indexer_make_website(local)) {
            SYSERROR("%s", "Site creation failed");
            exit(1);
        }
    }

    msg(STASIS_MSG_L1, "Copying indexed delivery to '%s'\n", destdir);
    char cmd[PATH_MAX];
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "rsync -ah%s --delete --exclude 'tmp/' --exclude 'tools/' '%s/' '%s/'", globals.verbose ? "v" : "q", workdir, destdir);
    guard_free(destdir);

    if (globals.verbose) {
        puts(cmd);
    }

    if (system(cmd)) {
        SYSERROR("%s", "Copy operation failed");
        rmtree(workdir);
        exit(1);
    }

    msg(STASIS_MSG_L1, "Removing work directory: %s\n", workdir);
    if (rmtree(workdir)) {
        SYSERROR("Failed to remove work directory: %s", strerror(errno));
    }

    guard_strlist_free(&metafiles);
    delivery_free(&ctx);
    globals_free();
    msg(STASIS_MSG_L1, "Done!\n");
    return 0;
}
