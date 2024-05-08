#include <getopt.h>
#include <fnmatch.h>
#include "omc.h"

static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"destdir", required_argument, 0, 'd'},
        {"verbose", no_argument, 0, 'v'},
        {"unbuffered", no_argument, 0, 'U'},
        {0, 0, 0, 0},
};

const char *long_options_help[] = {
        "Display this usage statement",
        "Destination directory",
        "Increase output verbosity",
        "Disable line buffering",
        NULL,
};

static void usage(char *name) {
    int maxopts = sizeof(long_options) / sizeof(long_options[0]);
    unsigned char *opts = calloc(maxopts + 1, sizeof(char));
    for (int i = 0; i < maxopts; i++) {
        opts[i] = long_options[i].val;
    }
    printf("usage: %s [-%s] {{OMC_ROOT}...}\n", name, opts);
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
    char line[OMC_NAME_MAX] = {0};
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
        return -1;
    }
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

int micromamba(const char *write_to, const char *prefix, char *command, ...) {
    struct utsname sys;
    uname(&sys);

    tolower_s(sys.sysname);
    if (!strcmp(sys.sysname, "darwin")) {
        strcpy(sys.sysname, "osx");
    }

    if (!strcmp(sys.machine, "x86_64")) {
        strcpy(sys.machine, "64");
    }

    char url[PATH_MAX];
    sprintf(url, "https://micro.mamba.pm/api/micromamba/%s-%s/latest", sys.sysname, sys.machine);
    if (access("latest", F_OK)) {
        download(url, "latest", NULL);
    }

    char mmbin[PATH_MAX];
    sprintf(mmbin, "%s/micromamba", write_to);

    if (access(mmbin, F_OK)) {
        char untarcmd[PATH_MAX];
        mkdirs(write_to, 0755);
        sprintf(untarcmd, "tar -xvf latest -C %s --strip-components=1 bin/micromamba 1>/dev/null", write_to);
        system(untarcmd);
    }

    char cmd[OMC_BUFSIZ];
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "%s -r %s -p %s ", mmbin, prefix, prefix);
    va_list args;
    va_start(args, command);
    vsprintf(cmd + strlen(cmd), command, args);
    va_end(args);

    mkdirs(prefix, 0755);

    char rcpath[PATH_MAX];
    sprintf(rcpath, "%s/.condarc", prefix);
    touch(rcpath);

    setenv("CONDARC", rcpath, 1);
    setenv("MAMBA_ROOT_PREFIX", prefix, 1);
    int status = system(cmd);
    unsetenv("MAMBA_ROOT_PREFIX");

    return status;
}

int indexer_conda(struct Delivery *ctx) {
    int status = 0;
    char prefix[PATH_MAX];
    sprintf(prefix, "%s/%s", ctx->storage.tmpdir, "indexer");

    status += micromamba(ctx->storage.tmpdir, prefix, "config prepend --env channels conda-forge");
    if (!globals.verbose) {
        status += micromamba(ctx->storage.tmpdir, prefix, "config set --env quiet true");
    }
    status += micromamba(ctx->storage.tmpdir, prefix, "config set --env always_yes true");
    status += micromamba(ctx->storage.tmpdir, prefix, "install conda-build");
    status += micromamba(ctx->storage.tmpdir, prefix, "run conda index %s", ctx->storage.conda_artifact_dir);
    return status;
}

int indexer_readmes(struct Delivery ctx[], size_t nelem) {
    // Find unique architecture identifiers
    struct StrList *architectures = strlist_init();
    for (size_t i = 0; i < nelem; i++) {
        if (!strstr_array(architectures->data, ctx[i].system.arch)) {
            strlist_append(&architectures, ctx[i].system.arch);
        }
    }

    // Find unique platform identifiers
    struct StrList *platforms = strlist_init();
    for (size_t i = 0; i < nelem; i++) {
        if (!strstr_array(platforms->data, ctx[i].system.platform[DELIVERY_PLATFORM_RELEASE])) {
            strlist_append(&platforms, ctx[i].system.platform[DELIVERY_PLATFORM_RELEASE]);
        }
    }

    struct StrList *python_versions = strlist_init();
    for (size_t i = 0; i < nelem; i++) {
        if (!strstr_array(python_versions->data, ctx[i].meta.python_compact)) {
            strlist_append(&python_versions, ctx[i].meta.python_compact);
        }
    }
    strlist_reverse(python_versions);

    FILE *indexfp;
    char readmefile[PATH_MAX] = {0};
    sprintf(readmefile, "%s/README.md", ctx->storage.delivery_dir);

    indexfp = fopen(readmefile, "w+");
    if (!indexfp) {
        fprintf(stderr, "Unable to open %s for writing\n", readmefile);
        return -1;
    }

    fprintf(indexfp, "# %s %s\n", ctx->meta.name, ctx->meta.version);
    int latest = get_latest_rc(ctx, nelem);
    for (size_t i = 0; i < strlist_count(python_versions); i++) {
        struct StrList *files = NULL;
        char *python_version = strlist_item(python_versions, i);
        fprintf(indexfp, "## py%s\n", python_version);
        if (indexer_get_files(&files, ctx->storage.delivery_dir, "README-*-%d-*py%s*.md", latest, python_version)) {
            fprintf(stderr, "Unable to list directory\n");
            return -1;
        }
        for (size_t x = 0; x < strlist_count(files); x++) {
            char *fname = strlist_item(files, x);
            if (globals.verbose) {
                puts(fname);
            }
            fprintf(indexfp, "- [%s](%s)\n", fname, fname);
        }

        guard_strlist_free(&files);
    }
    fclose(indexfp);
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
    path_store(&ctx->storage.cfgdump_dir, PATH_MAX, ctx->storage.output_dir, "config");
    path_store(&ctx->storage.meta_dir, PATH_MAX, ctx->storage.output_dir, "meta");
    path_store(&ctx->storage.delivery_dir, PATH_MAX, ctx->storage.output_dir, "delivery");
    path_store(&ctx->storage.package_dir, PATH_MAX, ctx->storage.output_dir, "packages");
    path_store(&ctx->storage.wheel_artifact_dir, PATH_MAX, ctx->storage.package_dir, "wheels");
    path_store(&ctx->storage.conda_artifact_dir, PATH_MAX, ctx->storage.package_dir, "conda");
}

int main(int argc, char *argv[]) {
    size_t rootdirs_total = 0;
    char *destdir = NULL;
    char **rootdirs = NULL;
    int c = 0;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "hd:vU", long_options, &option_index)) != -1) {
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

    if (!rootdirs || !rootdirs_total) {
        fprintf(stderr, "You must specify at least one OMC root directory to index\n");
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
    char workdir_template[PATH_MAX];
    char *system_tmp = getenv("TMPDIR");
    if (system_tmp) {
        strcat(workdir_template, system_tmp);
    } else {
        strcat(workdir_template, "/tmp");
    }
    strcat(workdir_template, "/omc-combine.XXXXXX");
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

    indexer_init_dirs(&ctx, workdir);

    msg(OMC_MSG_L1, "Merging delivery root %s\n", rootdirs_total > 1 ? "directories" : "directory");
    if (indexer_combine_rootdirs(&ctx, workdir, rootdirs, rootdirs_total)) {
        SYSERROR("%s", "Copy operation failed");
        rmtree(workdir);
    }

    msg(OMC_MSG_L1, "Indexing conda packages\n");
    if (indexer_conda(&ctx)) {
        SYSERROR("%s", "Conda package indexing operation failed");
        exit(1);
    }

    msg(OMC_MSG_L1, "Indexing wheel packages\n");
    if (indexer_wheels(&ctx)) {
        SYSERROR("%s", "Python package indexing operation failed");
        exit(1);
    }

    msg(OMC_MSG_L1, "Loading metadata\n");
    struct StrList *metafiles = NULL;
    indexer_get_files(&metafiles, ctx.storage.meta_dir, "*.omc");
    strlist_sort(metafiles, OMC_SORT_LEN_ASCENDING);
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

    msg(OMC_MSG_L1, "Indexing README files\n");
    if (indexer_readmes(local, strlist_count(metafiles))) {
        SYSERROR("%s", "README file indexing operation failed");
        exit(1);
    }

    msg(OMC_MSG_L1, "Copying indexed delivery to '%s'\n", destdir);
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

    msg(OMC_MSG_L1, "Removing work directory: %s\n", workdir);
    if (rmtree(workdir)) {
        SYSERROR("Failed to remove work directory: %s", strerror(errno));
    }

    msg(OMC_MSG_L1, "Done!\n");
    return 0;
}