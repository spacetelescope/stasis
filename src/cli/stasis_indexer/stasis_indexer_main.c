#include <getopt.h>
#include "args.h"
#include "callbacks.h"
#include "helpers.h"
#include "junitxml_report.h"
#include "website.h"
#include "readmes.h"
#include "delivery.h"

int indexer_combine_rootdirs(const char *dest, char **rootdirs, const size_t rootdirs_total) {
    char cmd[PATH_MAX] = {0};
    char destdir_bare[PATH_MAX] = {0};
    char destdir_with_output[PATH_MAX] = {0};
    char *destdir = destdir_bare;

    strcpy(destdir_bare, dest);
    strcpy(destdir_with_output, dest);
    strcat(destdir_with_output, "/output");

    if (!access(destdir_with_output, F_OK)) {
        destdir = destdir_with_output;
    }

    snprintf(cmd, sizeof(cmd), "rsync -ah%s --delete --exclude 'tools/' --exclude 'tmp/' --exclude 'build/' ", globals.verbose ? "v" : "q");
    for (size_t i = 0; i < rootdirs_total; i++) {
        char srcdir_bare[PATH_MAX] = {0};
        char srcdir_with_output[PATH_MAX] = {0};
        char *srcdir = srcdir_bare;
        strcpy(srcdir_bare, rootdirs[i]);
        strcpy(srcdir_with_output, rootdirs[i]);
        strcat(srcdir_with_output, "/output");

        if (access(srcdir_bare, F_OK)) {
            fprintf(stderr, "%s does not exist\n", srcdir_bare);
            continue;
        }

        if (!access(srcdir_with_output, F_OK)) {
            srcdir = srcdir_with_output;
        }
        snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "'%s'/ ", srcdir);
    }
    snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), " %s/", destdir);

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

int indexer_conda(const struct Delivery *ctx, struct MicromambaInfo m) {
    int status = 0;

    status += micromamba(&m, "run conda index %s", ctx->storage.conda_artifact_dir);
    return status;
}

int indexer_symlinks(struct Delivery **ctx, const size_t nelem) {
    struct Delivery **data = NULL;
    size_t nelem_real = 0;
    data = get_latest_deliveries(ctx, nelem, &nelem_real);
    //int latest = get_latest_rc(ctx, nelem);

    if (!pushd((*ctx)->storage.delivery_dir)) {
        for (size_t i = 0; i < nelem_real; i++) {
            char link_name_spec[PATH_MAX];
            char link_name_readme[PATH_MAX];

            char file_name_spec[PATH_MAX];
            char file_name_readme[PATH_MAX];

            if (!data[i]->meta.name) {
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
        fprintf(stderr, "Unable to enter delivery directory: %s\n", (*ctx)->storage.delivery_dir);
        guard_free(data);
        return -1;
    }

    // "latest" is an array of pointers to ctx[]. Do not free the contents of the array.
    guard_free(data);
    return 0;
}

void indexer_init_dirs(struct Delivery *ctx, const char *workdir) {
    path_store(&ctx->storage.root, PATH_MAX, workdir, "");
    path_store(&ctx->storage.tmpdir, PATH_MAX, ctx->storage.root, "tmp");
    if (delivery_init_tmpdir(ctx)) {
        fprintf(stderr, "Failed to configure temporary storage directory\n");
        exit(1);
    }

    char *user_dir = expandpath("~/.stasis/indexer");
    if (!user_dir) {
        SYSERROR("%s", "expandpath failed");
    }

    path_store(&ctx->storage.output_dir, PATH_MAX, ctx->storage.root, "");
    path_store(&ctx->storage.tools_dir, PATH_MAX, user_dir, "tools");
    path_store(&globals.conda_install_prefix, PATH_MAX, user_dir, "conda");
    path_store(&ctx->storage.cfgdump_dir, PATH_MAX, ctx->storage.output_dir, "config");
    path_store(&ctx->storage.meta_dir, PATH_MAX, ctx->storage.output_dir, "meta");
    path_store(&ctx->storage.delivery_dir, PATH_MAX, ctx->storage.output_dir, "delivery");
    path_store(&ctx->storage.package_dir, PATH_MAX, ctx->storage.output_dir, "packages");
    path_store(&ctx->storage.results_dir, PATH_MAX, ctx->storage.output_dir, "results");
    path_store(&ctx->storage.wheel_artifact_dir, PATH_MAX, ctx->storage.package_dir, "wheels");
    path_store(&ctx->storage.conda_artifact_dir, PATH_MAX, ctx->storage.package_dir, "conda");
    path_store(&ctx->storage.docker_artifact_dir, PATH_MAX, ctx->storage.package_dir, "docker");
    guard_free(user_dir);

    char newpath[PATH_MAX] = {0};
    if (getenv("PATH")) {
        sprintf(newpath, "%s/bin:%s", ctx->storage.tools_dir, getenv("PATH"));
        setenv("PATH", newpath, 1);
    } else {
        SYSERROR("%s", "environment variable PATH is undefined. Unable to continue.");
        exit(1);
    }
}

int main(const int argc, char *argv[]) {
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
                if (mkdir(optarg, 0755)) {
                    if (errno != 0 && errno != EEXIST) {
                        SYSERROR("Unable to create destination directory, '%s': %s", optarg, strerror(errno));
                        exit(1);
                    }
                }
                destdir = realpath(optarg, NULL);
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

    const int current_index = optind;
    if (optind < argc) {
        rootdirs_total = argc - current_index;
        rootdirs = calloc(rootdirs_total + 1, sizeof(*rootdirs));

        int i = 0;
        while (optind < argc) {
            if (argv[optind]) {
                if (access(argv[optind], F_OK) < 0) {
                    fprintf(stderr, "%s: %s\n", argv[optind], strerror(errno));
                    exit(1);
                }
            }
            // use first positional argument
            rootdirs[i] = realpath(argv[optind], NULL);
            optind++;
            break;
        }
    }

    if (isempty(destdir)) {
        if (mkdir("output", 0755)) {
            if (errno != 0 && errno != EEXIST) {
                SYSERROR("Unable to create destination directory, '%s': %s", "output", strerror(errno));
                exit(1);
            }
        }
        destdir = realpath("output", NULL);
    }

    if (!rootdirs || !rootdirs_total) {
        fprintf(stderr, "You must specify at least one STASIS root directory to index\n");
        exit(1);
    }

    for (size_t i = 0; i < rootdirs_total; i++) {
        if (isempty(rootdirs[i]) || !strcmp(rootdirs[i], "/") || !strcmp(rootdirs[i], "\\")) {
            SYSERROR("Unsafe directory: %s", rootdirs[i]);
            exit(1);
        }

        if (access(rootdirs[i], F_OK)) {
            SYSERROR("%s: %s", rootdirs[i], strerror(errno));
            exit(1);
        }
    }

    char stasis_sysconfdir_tmp[PATH_MAX];
    if (getenv("STASIS_SYSCONFDIR")) {
        strncpy(stasis_sysconfdir_tmp, getenv("STASIS_SYSCONFDIR"), sizeof(stasis_sysconfdir_tmp) - 1);
    } else {
        strncpy(stasis_sysconfdir_tmp, STASIS_SYSCONFDIR, sizeof(stasis_sysconfdir_tmp) - 1);
    }

    globals.sysconfdir = realpath(stasis_sysconfdir_tmp, NULL);
    if (!globals.sysconfdir) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "Unable to resolve path to configuration directory: %s\n", stasis_sysconfdir_tmp);
        exit(1);
    }

    char workdir_template[PATH_MAX] = {0};
    const char *system_tmp = getenv("TMPDIR");
    if (system_tmp) {
        strcat(workdir_template, system_tmp);
    } else {
        strcat(workdir_template, "/tmp");
    }
    strcat(workdir_template, "/stasis-combine.XXXXXX");
    char *workdir = mkdtemp(workdir_template);
    if (!workdir) {
        SYSERROR("Unable to create temporary directory: %s", workdir_template);
        exit(1);
    }
    if (isempty(workdir) || !strcmp(workdir, "/") || !strcmp(workdir, "\\")) {
        SYSERROR("Unsafe directory: %s", workdir);
        exit(1);
    }

    struct Delivery ctx = {0};

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

    struct MicromambaInfo m;
    if (micromamba_configure(&ctx, &m)) {
        SYSERROR("%s", "Unable to configure micromamba");
        exit(1);
    }

    msg(STASIS_MSG_L1, "Indexing conda packages\n");
    if (indexer_conda(&ctx, m)) {
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
    get_files(&metafiles, ctx.storage.meta_dir, "*.stasis");
    strlist_sort(metafiles, STASIS_SORT_LEN_ASCENDING);

    struct Delivery **local = calloc(strlist_count(metafiles) + 1, sizeof(*local));
    if (!local) {
        SYSERROR("%s", "Unable to allocate bytes for local delivery context array");
        exit(1);
    }

    for (size_t i = 0; i < strlist_count(metafiles); i++) {
        char *item = strlist_item(metafiles, i);
        // Copy the pre-filled contents of the main delivery context
        local[i] = delivery_duplicate(&ctx);
        if (!local[i]) {
            SYSERROR("Unable to duplicate delivery context %zu", i);
            exit(1);
        }
        if (globals.verbose) {
            puts(item);
        }
        load_metadata(local[i], item);
    }
    qsort(local, strlist_count(metafiles), sizeof(*local), callback_sort_deliveries_cmpfn);

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

    const char *revisionfile = "REVISION";
    msg(STASIS_MSG_L1, "Writing revision file: %s\n", revisionfile);
    if (!pushd(workdir)) {
        FILE *revisionfp = fopen(revisionfile, "w+");
        if (!revisionfp) {
            SYSERROR("Unable to open revision file for writing: %s", revisionfile);
            rmtree(workdir);
            exit(1);
        }
        fprintf(revisionfp, "%d\n", get_latest_rc(local, strlist_count(metafiles)));
        fclose(revisionfp);
        popd();
    } else {
        SYSERROR("%s", workdir);
        rmtree(workdir);
        exit(1);
    }

    const char *manifestfile = "TREE";
    msg(STASIS_MSG_L1, "Writing manifest: %s\n", manifestfile);
    if (!pushd(workdir)) {
        FILE *manifestfp = fopen(manifestfile, "w+");
        if (!manifestfp) {
            SYSERROR("Unable to open manifest for writing: %s", manifestfile);
            rmtree(workdir);
            exit(1);
        }
        if (write_manifest(".", (char *[]) {"tools", "build", "tmp", NULL}, manifestfp)) {
            SYSERROR("Unable to write records to manifest: %s", manifestfile);
            rmtree(workdir);
            exit(1);
        }
        fclose(manifestfp);
        popd();
    } else {
        SYSERROR("%s", workdir);
        rmtree(workdir);
        exit(1);
    }

    msg(STASIS_MSG_L1, "Copying indexed delivery to '%s'\n", destdir);
    char cmd[PATH_MAX] = {0};
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

    guard_free(destdir);
    guard_array_free(rootdirs);
    guard_free(m.micromamba_prefix);
    delivery_free(&ctx);
    for (size_t i = 0; i < strlist_count(metafiles); i++) {
        delivery_free(local[i]);
        guard_free(local[i]);
    }
    guard_free(local);
    guard_strlist_free(&metafiles);
    globals_free();

    msg(STASIS_MSG_L1, "Done!\n");

    return 0;
}
