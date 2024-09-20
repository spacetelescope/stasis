#include "delivery.h"


const char *release_header = "# delivery_name: %s\n"
                             "# delivery_fmt: %s\n"
                             "# creation_time: %s\n"
                             "# conda_ident: %s\n"
                             "# conda_build_ident: %s\n";

char *delivery_get_release_header(struct Delivery *ctx) {
    char output[STASIS_BUFSIZ];
    char stamp[100];
    strftime(stamp, sizeof(stamp) - 1, "%c", ctx->info.time_info);
    sprintf(output, release_header,
            ctx->info.release_name,
            ctx->rules.release_fmt,
            stamp,
            ctx->conda.tool_version,
            ctx->conda.tool_build_version);
    return strdup(output);
}

int delivery_dump_metadata(struct Delivery *ctx) {
    FILE *fp;
    char filename[PATH_MAX];
    sprintf(filename, "%s/meta-%s.stasis", ctx->storage.meta_dir, ctx->info.release_name);
    fp = fopen(filename, "w+");
    if (!fp) {
        return -1;
    }
    if (globals.verbose) {
        printf("%s\n", filename);
    }
    fprintf(fp, "name %s\n", ctx->meta.name);
    fprintf(fp, "version %s\n", ctx->meta.version);
    fprintf(fp, "rc %d\n", ctx->meta.rc);
    fprintf(fp, "python %s\n", ctx->meta.python);
    fprintf(fp, "python_compact %s\n", ctx->meta.python_compact);
    fprintf(fp, "mission %s\n", ctx->meta.mission);
    fprintf(fp, "codename %s\n", ctx->meta.codename ? ctx->meta.codename : "");
    fprintf(fp, "platform %s %s %s %s\n",
            ctx->system.platform[DELIVERY_PLATFORM],
            ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR],
            ctx->system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER],
            ctx->system.platform[DELIVERY_PLATFORM_RELEASE]);
    fprintf(fp, "arch %s\n", ctx->system.arch);
    fprintf(fp, "time %s\n", ctx->info.time_str_epoch);
    fprintf(fp, "release_fmt %s\n", ctx->rules.release_fmt);
    fprintf(fp, "release_name %s\n", ctx->info.release_name);
    fprintf(fp, "build_name_fmt %s\n", ctx->rules.build_name_fmt);
    fprintf(fp, "build_name %s\n", ctx->info.build_name);
    fprintf(fp, "build_number_fmt %s\n", ctx->rules.build_number_fmt);
    fprintf(fp, "build_number %s\n", ctx->info.build_number);
    fprintf(fp, "conda_installer_baseurl %s\n", ctx->conda.installer_baseurl);
    fprintf(fp, "conda_installer_name %s\n", ctx->conda.installer_name);
    fprintf(fp, "conda_installer_version %s\n", ctx->conda.installer_version);
    fprintf(fp, "conda_installer_platform %s\n", ctx->conda.installer_platform);
    fprintf(fp, "conda_installer_arch %s\n", ctx->conda.installer_arch);

    fclose(fp);
    return 0;
}

void delivery_rewrite_spec(struct Delivery *ctx, char *filename, unsigned stage) {
    char output[PATH_MAX];
    char *header = NULL;
    char *tempfile = NULL;
    FILE *tp = NULL;

    if (stage == DELIVERY_REWRITE_SPEC_STAGE_1) {
        header = delivery_get_release_header(ctx);
        if (!header) {
            msg(STASIS_MSG_ERROR, "failed to generate release header string\n", filename);
            exit(1);
        }
        tempfile = xmkstemp(&tp, "w+");
        if (!tempfile || !tp) {
            msg(STASIS_MSG_ERROR, "%s: unable to create temporary file\n", strerror(errno));
            exit(1);
        }
        fprintf(tp, "%s", header);

        // Read the original file
        char **contents = file_readlines(filename, 0, 0, NULL);
        if (!contents) {
            msg(STASIS_MSG_ERROR, "%s: unable to read %s", filename);
            exit(1);
        }

        // Write temporary data
        for (size_t i = 0; contents[i] != NULL; i++) {
            if (startswith(contents[i], "channels:")) {
                // Allow for additional conda channel injection
                if (ctx->conda.conda_packages_defer && strlist_count(ctx->conda.conda_packages_defer)) {
                    fprintf(tp, "%s  - @CONDA_CHANNEL@\n", contents[i]);
                    continue;
                }
            } else if (strstr(contents[i], "- pip:")) {
                if (ctx->conda.pip_packages_defer && strlist_count(ctx->conda.pip_packages_defer)) {
                    // Allow for additional pip argument injection
                    fprintf(tp, "%s      - @PIP_ARGUMENTS@\n", contents[i]);
                    continue;
                }
            } else if (startswith(contents[i], "prefix:")) {
                // Remove the prefix key
                if (strstr(contents[i], "/") || strstr(contents[i], "\\")) {
                    // path is on the same line as the key
                    continue;
                } else {
                    // path is on the next line?
                    if (contents[i + 1] && (strstr(contents[i + 1], "/") || strstr(contents[i + 1], "\\"))) {
                        i++;
                    }
                    continue;
                }
            }
            fprintf(tp, "%s", contents[i]);
        }
        GENERIC_ARRAY_FREE(contents);
        guard_free(header);
        fflush(tp);
        fclose(tp);

        // Replace the original file with our temporary data
        if (copy2(tempfile, filename, CT_PERM) < 0) {
            fprintf(stderr, "%s: could not rename '%s' to '%s'\n", strerror(errno), tempfile, filename);
            exit(1);
        }
        remove(tempfile);
        guard_free(tempfile);
    } else if (globals.enable_rewrite_spec_stage_2 && stage == DELIVERY_REWRITE_SPEC_STAGE_2) {
        // Replace "local" channel with the staging URL
        if (ctx->storage.conda_staging_url) {
            file_replace_text(filename, "@CONDA_CHANNEL@", ctx->storage.conda_staging_url, 0);
        } else if (globals.jfrog.repo) {
            sprintf(output, "%s/%s/%s/%s/packages/conda", globals.jfrog.url, globals.jfrog.repo, ctx->meta.mission, ctx->info.build_name);
            file_replace_text(filename, "@CONDA_CHANNEL@", output, 0);
        } else {
            msg(STASIS_MSG_WARN, "conda_staging_dir is not configured. Using fallback: '%s'\n", ctx->storage.conda_artifact_dir);
            file_replace_text(filename, "@CONDA_CHANNEL@", ctx->storage.conda_artifact_dir, 0);
        }

        if (ctx->storage.wheel_staging_url) {
            file_replace_text(filename, "@PIP_ARGUMENTS@", ctx->storage.wheel_staging_url, 0);
        } else if (globals.enable_artifactory && globals.jfrog.url && globals.jfrog.repo) {
            sprintf(output, "--extra-index-url %s/%s/%s/%s/packages/wheels", globals.jfrog.url, globals.jfrog.repo, ctx->meta.mission, ctx->info.build_name);
            file_replace_text(filename, "@PIP_ARGUMENTS@", output, 0);
        } else {
            msg(STASIS_MSG_WARN, "wheel_staging_dir is not configured. Using fallback: '%s'\n", ctx->storage.wheel_artifact_dir);
            sprintf(output, "--extra-index-url file://%s", ctx->storage.wheel_artifact_dir);
            file_replace_text(filename, "@PIP_ARGUMENTS@", output, 0);
        }
    }
}

int delivery_copy_conda_artifacts(struct Delivery *ctx) {
    char cmd[STASIS_BUFSIZ];
    char conda_build_dir[PATH_MAX];
    char subdir[PATH_MAX];
    memset(cmd, 0, sizeof(cmd));
    memset(conda_build_dir, 0, sizeof(conda_build_dir));
    memset(subdir, 0, sizeof(subdir));

    sprintf(conda_build_dir, "%s/%s", ctx->storage.conda_install_prefix, "conda-bld");
    // One must run conda build at least once to create the "conda-bld" directory.
    // When this directory is missing there can be no build artifacts.
    if (access(conda_build_dir, F_OK) < 0) {
        msg(STASIS_MSG_RESTRICT | STASIS_MSG_WARN | STASIS_MSG_L3,
            "Skipped: 'conda build' has never been executed.\n");
        return 0;
    }

    snprintf(cmd, sizeof(cmd) - 1, "rsync -avi --progress %s/%s %s",
             conda_build_dir,
             ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR],
             ctx->storage.conda_artifact_dir);

    return system(cmd);
}

int delivery_index_conda_artifacts(struct Delivery *ctx) {
    return conda_index(ctx->storage.conda_artifact_dir);
}

int delivery_copy_wheel_artifacts(struct Delivery *ctx) {
    char cmd[PATH_MAX];
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd) - 1, "rsync -avi --progress %s/*/dist/*.whl %s",
             ctx->storage.build_sources_dir,
             ctx->storage.wheel_artifact_dir);
    return system(cmd);
}

int delivery_index_wheel_artifacts(struct Delivery *ctx) {
    struct dirent *rec;
    DIR *dp;
    FILE *top_fp;

    dp = opendir(ctx->storage.wheel_artifact_dir);
    if (!dp) {
        return -1;
    }

    // Generate a "dumb" local pypi index that is compatible with:
    // pip install --extra-index-url
    char top_index[PATH_MAX];
    memset(top_index, 0, sizeof(top_index));
    sprintf(top_index, "%s/index.html", ctx->storage.wheel_artifact_dir);
    top_fp = fopen(top_index, "w+");
    if (!top_fp) {
        return -2;
    }

    while ((rec = readdir(dp)) != NULL) {
        // skip directories
        if (DT_REG == rec->d_type || !strcmp(rec->d_name, "..") || !strcmp(rec->d_name, ".")) {
            continue;
        }

        FILE *bottom_fp;
        char bottom_index[PATH_MAX * 2];
        memset(bottom_index, 0, sizeof(bottom_index));
        sprintf(bottom_index, "%s/%s/index.html", ctx->storage.wheel_artifact_dir, rec->d_name);
        bottom_fp = fopen(bottom_index, "w+");
        if (!bottom_fp) {
            return -3;
        }

        if (globals.verbose) {
            printf("+ %s\n", rec->d_name);
        }
        // Add record to top level index
        fprintf(top_fp, "<a href=\"%s/\">%s</a><br/>\n", rec->d_name, rec->d_name);

        char dpath[PATH_MAX * 2];
        memset(dpath, 0, sizeof(dpath));
        sprintf(dpath, "%s/%s", ctx->storage.wheel_artifact_dir, rec->d_name);
        struct StrList *packages = listdir(dpath);
        if (!packages) {
            fclose(top_fp);
            fclose(bottom_fp);
            return -4;
        }

        for (size_t i = 0; i < strlist_count(packages); i++) {
            char *package = strlist_item(packages, i);
            if (!endswith(package, ".whl")) {
                continue;
            }
            if (globals.verbose) {
                printf("`- %s\n", package);
            }
            // Write record to bottom level index
            fprintf(bottom_fp, "<a href=\"%s\">%s</a><br/>\n", package, package);
        }
        fclose(bottom_fp);

        guard_strlist_free(&packages);
    }
    closedir(dp);
    fclose(top_fp);
    return 0;
}