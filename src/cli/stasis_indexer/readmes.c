#include "core.h"
#include "readmes.h"

int indexer_readmes(struct Delivery ctx[], const size_t nelem) {
    struct Delivery *latest = NULL;
    latest = get_latest_deliveries(ctx, nelem);

    char indexfile[PATH_MAX] = {0};
    sprintf(indexfile, "%s/README.md", ctx->storage.delivery_dir);

    if (!pushd(ctx->storage.delivery_dir)) {
        FILE *indexfp = fopen(indexfile, "w+");
        if (!indexfp) {
            fprintf(stderr, "Unable to open %s for writing\n", indexfile);
            return -1;
        }
        struct StrList *archs = get_architectures(latest, nelem);
        struct StrList *platforms = get_platforms(latest, nelem);

        fprintf(indexfp, "# %s-%s\n\n", ctx->meta.name, ctx->meta.version);
        fprintf(indexfp, "## Current Release\n\n");
        for (size_t p = 0; p < strlist_count(platforms); p++) {
            char *platform = strlist_item(platforms, p);
            for (size_t a = 0; a < strlist_count(archs); a++) {
                char *arch = strlist_item(archs, a);
                int have_combo = 0;
                for (size_t i = 0; i < nelem; i++) {
                    if (latest[i].system.platform) {
                        if (strstr(latest[i].system.platform[DELIVERY_PLATFORM_RELEASE], platform) &&
                            strstr(latest[i].system.arch, arch)) {
                            have_combo = 1;
                        }
                    }
                }
                if (!have_combo) {
                    continue;
                }
                fprintf(indexfp, "### %s-%s\n\n", platform, arch);
                for (size_t i = 0; i < nelem; i++) {
                    char link_name[PATH_MAX];
                    char readme_name[PATH_MAX];
                    char conf_name[PATH_MAX];
                    char conf_name_relative[PATH_MAX];
                    if (!latest[i].meta.name) {
                        continue;
                    }
                    sprintf(link_name, "latest-py%s-%s-%s.yml", latest[i].meta.python_compact, latest[i].system.platform[DELIVERY_PLATFORM_RELEASE], latest[i].system.arch);
                    sprintf(readme_name, "README-py%s-%s-%s.md", latest[i].meta.python_compact, latest[i].system.platform[DELIVERY_PLATFORM_RELEASE], latest[i].system.arch);
                    sprintf(conf_name, "%s.ini", latest[i].info.release_name);
                    sprintf(conf_name_relative, "../config/%s.ini", latest[i].info.release_name);
                    if (strstr(link_name, platform) && strstr(link_name, arch)) {
                        fprintf(indexfp, "- Info: [README](%s)\n", readme_name);
                        fprintf(indexfp, "- Release: [Conda Environment YAML](%s)\n", link_name);
                        fprintf(indexfp, "- Receipt: [STASIS input file](%s)\n", conf_name_relative);
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
            fprintf(indexfp, "- Receipt: [STASIS input file](%s.ini)\n", current->info.release_name);
        }
        fprintf(indexfp, "\n");

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
