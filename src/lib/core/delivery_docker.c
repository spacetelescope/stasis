#include "delivery.h"

int delivery_docker(struct Delivery *ctx) {
    if (!docker_capable(&ctx->deploy.docker.capabilities)) {
        return -1;
    }
    char tag[STASIS_NAME_MAX];
    char args[PATH_MAX];
    int has_registry = ctx->deploy.docker.registry != NULL;
    size_t total_tags = strlist_count(ctx->deploy.docker.tags);
    size_t total_build_args = strlist_count(ctx->deploy.docker.build_args);

    if (!has_registry) {
        msg(STASIS_MSG_WARN | STASIS_MSG_L2, "No docker registry defined. You will need to manually re-tag the resulting image.\n");
    }

    if (!total_tags) {
        char default_tag[PATH_MAX];
        msg(STASIS_MSG_WARN | STASIS_MSG_L2, "No docker tags defined by configuration. Generating default tag(s).\n");
        // generate local tag
        memset(default_tag, 0, sizeof(default_tag));
        sprintf(default_tag, "%s:%s-py%s", ctx->meta.name, ctx->info.build_name, ctx->meta.python_compact);
        tolower_s(default_tag);

        // Add tag
        ctx->deploy.docker.tags = strlist_init();
        strlist_append(&ctx->deploy.docker.tags, default_tag);

        if (has_registry) {
            // generate tag for target registry
            memset(default_tag, 0, sizeof(default_tag));
            sprintf(default_tag, "%s/%s:%s-py%s", ctx->deploy.docker.registry, ctx->meta.name, ctx->info.build_number, ctx->meta.python_compact);
            tolower_s(default_tag);

            // Add tag
            strlist_append(&ctx->deploy.docker.tags, default_tag);
        }
        // regenerate total tag available
        total_tags = strlist_count(ctx->deploy.docker.tags);
    }

    memset(args, 0, sizeof(args));

    // Append image tags to command
    for (size_t i = 0; i < total_tags; i++) {
        char *tag_orig = strlist_item(ctx->deploy.docker.tags, i);
        strcpy(tag, tag_orig);
        docker_sanitize_tag(tag);
        sprintf(args + strlen(args), " -t \"%s\" ", tag);
    }

    // Append build arguments to command (i.e. --build-arg "key=value"
    for (size_t i = 0; i < total_build_args; i++) {
        char *build_arg = strlist_item(ctx->deploy.docker.build_args, i);
        if (!build_arg) {
            break;
        }
        sprintf(args + strlen(args), " --build-arg \"%s\" ", build_arg);
    }

    // Build the image
    char delivery_file[PATH_MAX] = {0};
    char dest[PATH_MAX] = {0};
    char rsync_cmd[PATH_MAX * 2] = {0};
    memset(delivery_file, 0, sizeof(delivery_file));
    memset(dest, 0, sizeof(dest));

    sprintf(delivery_file, "%s/%s.yml", ctx->storage.delivery_dir, ctx->info.release_name);
    if (access(delivery_file, F_OK) < 0) {
        fprintf(stderr, "docker build cannot proceed without delivery file: %s\n", delivery_file);
        return -1;
    }

    sprintf(dest, "%s/%s.yml", ctx->storage.build_docker_dir, ctx->info.release_name);
    if (copy2(delivery_file, dest, CT_PERM)) {
        fprintf(stderr, "Failed to copy delivery file to %s: %s\n", dest, strerror(errno));
        return -1;
    }

    memset(dest, 0, sizeof(dest));
    sprintf(dest, "%s/packages", ctx->storage.build_docker_dir);

    msg(STASIS_MSG_L2, "Copying conda packages\n");
    memset(rsync_cmd, 0, sizeof(rsync_cmd));
    sprintf(rsync_cmd, "rsync -avi --progress '%s' '%s'", ctx->storage.conda_artifact_dir, dest);
    if (system(rsync_cmd)) {
        fprintf(stderr, "Failed to copy conda artifacts to docker build directory\n");
        return -1;
    }

    msg(STASIS_MSG_L2, "Copying wheel packages\n");
    memset(rsync_cmd, 0, sizeof(rsync_cmd));
    sprintf(rsync_cmd, "rsync -avi --progress '%s' '%s'", ctx->storage.wheel_artifact_dir, dest);
    if (system(rsync_cmd)) {
        fprintf(stderr, "Failed to copy wheel artifactory to docker build directory\n");
    }

    if (docker_build(ctx->storage.build_docker_dir, args, ctx->deploy.docker.capabilities.build)) {
        return -1;
    }

    // Test the image
    // All tags point back to the same image so test the first one we see
    // regardless of how many are defined
    strcpy(tag, strlist_item(ctx->deploy.docker.tags, 0));
    docker_sanitize_tag(tag);

    msg(STASIS_MSG_L2, "Executing image test script for %s\n", tag);
    if (ctx->deploy.docker.test_script) {
        if (isempty(ctx->deploy.docker.test_script)) {
            msg(STASIS_MSG_L2 | STASIS_MSG_WARN, "Image test script has no content\n");
        } else {
            int state;
            if ((state = docker_script(tag, ctx->deploy.docker.test_script, 0))) {
                msg(STASIS_MSG_L2 | STASIS_MSG_ERROR, "Non-zero exit (%d) from test script. %s image archive will not be generated.\n", state >> 8, tag);
                // test failed -- don't save the image
                return -1;
            }
        }
    } else {
        msg(STASIS_MSG_L2 | STASIS_MSG_WARN, "No image test script defined\n");
    }

    // Test successful, save image
    if (docker_save(path_basename(tag), ctx->storage.docker_artifact_dir, ctx->deploy.docker.image_compression)) {
        // save failed
        return -1;
    }

    return 0;
}

