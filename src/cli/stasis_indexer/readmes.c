#include "core.h"
#include "readmes.h"

int indexer_readmes(struct Delivery ctx[], const size_t nelem) {
    struct Delivery *latest_deliveries = get_latest_deliveries(ctx, nelem);
    if (!latest_deliveries) {
        if (errno) {
            return -1;
        }
        return 0;
    }

    char indexfile[PATH_MAX] = {0};
    sprintf(indexfile, "%s/README.md", ctx->storage.delivery_dir);

    FILE *indexfp = fopen(indexfile, "w+");
    if (!indexfp) {
        fprintf(stderr, "Unable to open %s for writing\n", indexfile);
        return -1;
    }
    struct StrList *archs = get_architectures(latest_deliveries, nelem);
    struct StrList *platforms = get_platforms(latest_deliveries, nelem);

    fprintf(indexfp, "# %s-%s\n\n", ctx->meta.name, ctx->meta.version);
    fprintf(indexfp, "## Current Release\n\n");
    for (size_t p = 0; p < strlist_count(platforms); p++) {
        char *platform = strlist_item(platforms, p);
        for (size_t a = 0; a < strlist_count(archs); a++) {
            char *arch = strlist_item(archs, a);
            int have_combo = 0;
            for (size_t i = 0; i < nelem; i++) {
                if (latest_deliveries[i].system.platform) {
                    if (strstr(latest_deliveries[i].system.platform[DELIVERY_PLATFORM_RELEASE], platform) &&
                        strstr(latest_deliveries[i].system.arch, arch)) {
                        have_combo = 1;
                    }
                }
            }
            if (!have_combo) {
                continue;
            }
            fprintf(indexfp, "### %s-%s\n\n", platform, arch);
            for (size_t i = 0; i < nelem; i++) {
                char link_name[PATH_MAX] = {0};
                char readme_name[PATH_MAX] = {0};
                char conf_name[PATH_MAX] = {0};
                char conf_name_relative[PATH_MAX] = {0};
                if (!latest_deliveries[i].meta.name) {
                    continue;
                }
                sprintf(link_name, "latest-py%s-%s-%s.yml", latest_deliveries[i].meta.python_compact, latest_deliveries[i].system.platform[DELIVERY_PLATFORM_RELEASE], latest_deliveries[i].system.arch);
                sprintf(readme_name, "README-py%s-%s-%s.md", latest_deliveries[i].meta.python_compact, latest_deliveries[i].system.platform[DELIVERY_PLATFORM_RELEASE], latest_deliveries[i].system.arch);
                sprintf(conf_name, "%s.ini", latest_deliveries[i].info.release_name);
                sprintf(conf_name_relative, "../config/%s.ini", latest_deliveries[i].info.release_name);
                if (strstr(link_name, platform) && strstr(link_name, arch)) {
                    fprintf(indexfp, "- Python %s\n", latest_deliveries[i].meta.python);
                    fprintf(indexfp, "  - Info: [README](%s)\n", readme_name);
                    fprintf(indexfp, "  - Release: [Conda Environment YAML](%s)\n", link_name);
                    fprintf(indexfp, "  - Receipt: [STASIS input file](%s)\n", conf_name_relative);
                    fprintf(indexfp, "  - Docker: ");
                    struct StrList *docker_images = get_docker_images(&latest_deliveries[i], "");
                    if (docker_images
                        && strlist_count(docker_images)
                        && !strcmp(latest_deliveries[i].system.platform[DELIVERY_PLATFORM_RELEASE], "linux")) {
                        fprintf(indexfp, "[Archive](../packages/docker/%s)\n", path_basename(strlist_item(docker_images, 0)));
                    } else {
                        fprintf(indexfp, "N/A\n");
                    }
                    guard_free(docker_images);
                }
            }
            fprintf(indexfp, "\n");
        }
        fprintf(indexfp, "\n");
    }

    fprintf(indexfp, "## Releases\n");
    for (size_t i = 0; ctx[i].meta.name != NULL; i++) {
        struct Delivery *current = &ctx[i];
        fprintf(indexfp, "### %s\n", current->info.release_name);
        fprintf(indexfp, "- Info: [README](README-%s.html)\n", current->info.release_name);
        fprintf(indexfp, "- Release: [Conda Environment YAML](%s.yml)\n", current->info.release_name);
        fprintf(indexfp, "- Receipt: [STASIS input file](../config/%s.ini)\n", current->info.release_name);
        fprintf(indexfp, "- Docker: \n");

        char *pattern = NULL;
        asprintf(&pattern, "*%s*", current->info.build_number);
        if (!pattern) {
            SYSERROR("%s", "Unable to allocate bytes for pattern");
            return -1;
        }

        struct StrList *docker_images = get_docker_images(current, pattern);
        if (docker_images
                && strlist_count(docker_images)
                && !strcmp(current->system.platform[DELIVERY_PLATFORM_RELEASE], "linux")) {
            fprintf(indexfp, "[Archive](../packages/docker/%s)\n", path_basename(strlist_item(docker_images, 0)));
        } else {
            fprintf(indexfp, "N/A\n");
        }
        guard_free(docker_images);
        guard_free(pattern);
    }
    fprintf(indexfp, "\n");

    guard_strlist_free(&archs);
    guard_strlist_free(&platforms);
    fclose(indexfp);

    // "latest_deliveries" is an array of pointers to ctxs[]. Do not free the contents of the array.
    guard_free(latest_deliveries);
    return 0;
}
