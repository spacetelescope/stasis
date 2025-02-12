#include "delivery.h"

int delivery_init_artifactory(struct Delivery *ctx) {
    int status = 0;
    char dest[PATH_MAX] = {0};
    char filepath[PATH_MAX] = {0};
    snprintf(dest, sizeof(dest) - 1, "%s/bin", ctx->storage.tools_dir);
    snprintf(filepath, sizeof(dest) - 1, "%s/bin/jf", ctx->storage.tools_dir);

    if (!access(filepath, F_OK)) {
        // already have it
        msg(STASIS_MSG_L3, "Skipped download, %s already exists\n", filepath);
        goto delivery_init_artifactory_envsetup;
    }

    char *platform = ctx->system.platform[DELIVERY_PLATFORM];
    msg(STASIS_MSG_L3, "Downloading %s for %s %s\n", globals.jfrog.remote_filename, platform, ctx->system.arch);
    if ((status = artifactory_download_cli(dest,
                                           globals.jfrog.jfrog_artifactory_base_url,
                                           globals.jfrog.jfrog_artifactory_product,
                                           globals.jfrog.cli_major_ver,
                                           globals.jfrog.version,
                                           platform,
                                           ctx->system.arch,
                                           globals.jfrog.remote_filename))) {
        remove(filepath);
    }

    delivery_init_artifactory_envsetup:
    // CI (ridiculously generic, why?) disables interactive prompts and progress bar output
    setenv("CI", "1", 1);

    // JFROG_CLI_HOME_DIR is where .jfrog is stored
    char path[PATH_MAX] = {0};
    snprintf(path, sizeof(path) - 1, "%s/.jfrog", ctx->storage.build_dir);
    setenv("JFROG_CLI_HOME_DIR", path, 1);

    // JFROG_CLI_TEMP_DIR is where the obvious is stored
    setenv("JFROG_CLI_TEMP_DIR", ctx->storage.tmpdir, 1);
    return status;
}

int delivery_artifact_upload(struct Delivery *ctx) {
    int status = 0;

    if (jfrt_auth_init(&ctx->deploy.jfrog_auth)) {
        fprintf(stderr, "Failed to initialize Artifactory authentication context\n");
        return -1;
    }

    for (size_t i = 0; i < sizeof(ctx->deploy.jfrog) / sizeof(*ctx->deploy.jfrog); i++) {
        if (!ctx->deploy.jfrog[i].files || !ctx->deploy.jfrog[i].dest) {
            break;
        }
        jfrt_upload_init(&ctx->deploy.jfrog[i].upload_ctx);

        if (!globals.jfrog.repo) {
            msg(STASIS_MSG_WARN, "Artifactory repository path is not configured!\n");
            fprintf(stderr, "set STASIS_JF_REPO environment variable...\nOr append to configuration file:\n\n");
            fprintf(stderr, "[deploy:artifactory]\nrepo = example/generic/repo/path\n\n");
            status++;
            break;
        } else if (!ctx->deploy.jfrog[i].repo) {
            ctx->deploy.jfrog[i].repo = strdup(globals.jfrog.repo);
        }

        if (!ctx->deploy.jfrog[i].repo || isempty(ctx->deploy.jfrog[i].repo) || !strlen(ctx->deploy.jfrog[i].repo)) {
            // Unlikely to trigger if the config parser is working correctly
            msg(STASIS_MSG_ERROR, "Artifactory repository path is empty. Cannot continue.\n");
            status++;
            break;
        }

        ctx->deploy.jfrog[i].upload_ctx.workaround_parent_only = true;
        ctx->deploy.jfrog[i].upload_ctx.build_name = ctx->info.build_name;
        ctx->deploy.jfrog[i].upload_ctx.build_number = ctx->info.build_number;

        if (jfrog_cli_rt_ping(&ctx->deploy.jfrog_auth)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Unable to contact artifactory server: %s\n", ctx->deploy.jfrog_auth.url);
            return -1;
        }

        if (strlist_count(ctx->deploy.jfrog[i].files)) {
            for (size_t f = 0; f < strlist_count(ctx->deploy.jfrog[i].files); f++) {
                char dest[PATH_MAX] = {0};
                char files[PATH_MAX] = {0};
                snprintf(dest, sizeof(dest) - 1, "%s/%s", ctx->deploy.jfrog[i].repo, ctx->deploy.jfrog[i].dest);
                snprintf(files, sizeof(files) - 1, "%s", strlist_item(ctx->deploy.jfrog[i].files, f));
                status += jfrog_cli_rt_upload(&ctx->deploy.jfrog_auth, &ctx->deploy.jfrog[i].upload_ctx, files, dest);
            }
        }
    }

    if (globals.enable_artifactory_build_info) {
        if (!status && ctx->deploy.jfrog[0].files && ctx->deploy.jfrog[0].dest) {
            jfrog_cli_rt_build_collect_env(&ctx->deploy.jfrog_auth, ctx->deploy.jfrog[0].upload_ctx.build_name,
                                           ctx->deploy.jfrog[0].upload_ctx.build_number);
            jfrog_cli_rt_build_publish(&ctx->deploy.jfrog_auth, ctx->deploy.jfrog[0].upload_ctx.build_name,
                                       ctx->deploy.jfrog[0].upload_ctx.build_number);
        }
    } else {
        msg(STASIS_MSG_WARN | STASIS_MSG_L2, "Artifactory build info upload is disabled by CLI argument\n");
    }

    return status;
}

int delivery_mission_render_files(struct Delivery *ctx) {
    if (!ctx->storage.mission_dir) {
        fprintf(stderr, "Mission directory is not configured. Context not initialized?\n");
        return -1;
    }
    struct Data {
        char *src;
        char *dest;
    } data;
    struct INIFILE *cfg = ctx->_stasis_ini_fp.mission;

    memset(&data, 0, sizeof(data));
    data.src = calloc(PATH_MAX, sizeof(*data.src));
    if (!data.src) {
        perror("data.src");
        return -1;
    }

    for (size_t i = 0; i < cfg->section_count; i++) {
        union INIVal val;
        char *section_name = cfg->section[i]->key;
        if (!startswith(section_name, "template:")) {
            continue;
        }
        val.as_char_p = strchr(section_name, ':') + 1;
        if (val.as_char_p && isempty(val.as_char_p)) {
            guard_free(data.src);
            return 1;
        }
        sprintf(data.src, "%s/%s/%s", ctx->storage.mission_dir, ctx->meta.mission, val.as_char_p);
        msg(STASIS_MSG_L2, "%s\n", data.src);

        int err = 0;
        data.dest = ini_getval_str(cfg, section_name, "destination", INI_READ_RENDER, &err);

        struct stat st;
        if (lstat(data.src, &st)) {
            perror(data.src);
            guard_free(data.dest);
            continue;
        }

        char *contents = calloc(st.st_size + 1, sizeof(*contents));
        if (!contents) {
            perror("template file contents");
            guard_free(data.dest);
            continue;
        }

        FILE *fp = fopen(data.src, "rb");
        if (!fp) {
            perror(data.src);
            guard_free(contents);
            guard_free(data.dest);
            continue;
        }

        if (fread(contents, st.st_size, sizeof(*contents), fp) < 1) {
            perror("while reading template file");
            guard_free(contents);
            guard_free(data.dest);
            fclose(fp);
            continue;
        }
        fclose(fp);

        msg(STASIS_MSG_L3, "Writing %s\n", data.dest);
        if (tpl_render_to_file(contents, data.dest)) {
            guard_free(contents);
            guard_free(data.dest);
            continue;
        }
        guard_free(contents);
        guard_free(data.dest);
    }

    guard_free(data.src);
    return 0;
}

int delivery_series_sync(struct Delivery *ctx) {
    struct JFRT_Download dl = {0};

    if (jfrt_auth_init(&ctx->deploy.jfrog_auth)) {
        fprintf(stderr, "Failed to initialize Artifactory authentication context\n");
        return -1;  // error
    }

    char *r_fmt = strdup(ctx->rules.release_fmt);
    if (!r_fmt) {
        SYSERROR("%s", "Unable to allocate bytes for release format string");
        return -1;
    }

    // Replace revision formatters with wildcards
    const char *to_wild[] = {"%r", "%R"};
    for (size_t i = 0; i < sizeof(to_wild) / sizeof(*to_wild); i++) {
        const char *formatter = to_wild[i];
        if (replace_text(r_fmt, formatter, "*", 0) < 0) {
            SYSERROR("Failed to replace '%s' in delivery format string", formatter);
            return -1;
        }
    }

    char *release_pattern = NULL;
    if (delivery_format_str(ctx, &release_pattern, r_fmt) < 0) {
        SYSERROR("Unable to render delivery format string: %s", r_fmt);
        guard_free(r_fmt);
        return -1;
    }
    guard_free(r_fmt);

    char *remote_dir = NULL;
    if (asprintf(&remote_dir, "%s/%s/%s/(*/*%s*)",
                 globals.jfrog.repo,
                 ctx->meta.mission,
                 ctx->info.build_name,
                 release_pattern) < 0) {
        SYSERROR("%s", "Unable to allocate bytes for remote directory path");
        guard_free(release_pattern);
        return -1;
    }
    guard_free(release_pattern);

    char *dest_dir = NULL;
    if (asprintf(&dest_dir, "%s/{1}", ctx->storage.output_dir) < 0) {
        SYSERROR("%s", "Unable to allocate bytes for destination directory path");
        return -1;
    }

    int status = jfrog_cli_rt_download(&ctx->deploy.jfrog_auth, &dl, remote_dir, dest_dir);
    guard_free(dest_dir);
    guard_free(remote_dir);
    return status;
}
