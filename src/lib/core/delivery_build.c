#include "delivery.h"

int delivery_build_recipes(struct Delivery *ctx) {
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        char *recipe_dir = NULL;
        if (ctx->tests[i].build_recipe) { // build a conda recipe
            int recipe_type;
            int status;
            if (recipe_clone(ctx->storage.build_recipes_dir, ctx->tests[i].build_recipe, NULL, &recipe_dir)) {
                fprintf(stderr, "Encountered an issue while cloning recipe for: %s\n", ctx->tests[i].name);
                return -1;
            }
            if (!recipe_dir) {
                fprintf(stderr, "BUG: recipe_clone() succeeded but recipe_dir is NULL: %s\n", strerror(errno));
                return -1;
            }
            recipe_type = recipe_get_type(recipe_dir);
            if(!pushd(recipe_dir)) {
                if (RECIPE_TYPE_ASTROCONDA == recipe_type) {
                    pushd(path_basename(ctx->tests[i].repository));
                } else if (RECIPE_TYPE_CONDA_FORGE == recipe_type) {
                    pushd("recipe");
                }

                char recipe_version[100];
                char recipe_buildno[100];
                char recipe_git_url[PATH_MAX];
                char recipe_git_rev[PATH_MAX];

                //sprintf(recipe_version, "{%% set version = GIT_DESCRIBE_TAG ~ \".dev\" ~ GIT_DESCRIBE_NUMBER ~ \"+\" ~ GIT_DESCRIBE_HASH %%}");
                //sprintf(recipe_git_url, "  git_url: %s", ctx->tests[i].repository);
                //sprintf(recipe_git_rev, "  git_rev: %s", ctx->tests[i].version);
                // TODO: Conditionally download archives if github.com is the origin. Else, use raw git_* keys ^^^
                sprintf(recipe_version, "{%% set version = \"%s\" %%}", ctx->tests[i].repository_info_tag ? ctx->tests[i].repository_info_tag : ctx->tests[i].version);
                sprintf(recipe_git_url, "  url: %s/archive/refs/tags/{{ version }}.tar.gz", ctx->tests[i].repository);
                strcpy(recipe_git_rev, "");
                sprintf(recipe_buildno, "  number: 0");

                unsigned flags = REPLACE_TRUNCATE_AFTER_MATCH;
                //file_replace_text("meta.yaml", "{% set version = ", recipe_version);
                if (ctx->meta.final) { // remove this. i.e. statis cannot deploy a release to conda-forge
                    sprintf(recipe_version, "{%% set version = \"%s\" %%}", ctx->tests[i].version);
                    // TODO: replace sha256 of tagged archive
                    // TODO: leave the recipe unchanged otherwise. in theory this should produce the same conda package hash as conda forge.
                    // For now, remove the sha256 requirement
                    file_replace_text("meta.yaml", "sha256:", "\n", flags);
                } else {
                    file_replace_text("meta.yaml", "{% set version = ", recipe_version, flags);
                    file_replace_text("meta.yaml", "  url:", recipe_git_url, flags);
                    //file_replace_text("meta.yaml", "sha256:", recipe_git_rev);
                    file_replace_text("meta.yaml", "  sha256:", "\n", flags);
                    file_replace_text("meta.yaml", "  number:", recipe_buildno, flags);
                }

                char command[PATH_MAX];
                if (RECIPE_TYPE_CONDA_FORGE == recipe_type) {
                    char arch[STASIS_NAME_MAX] = {0};
                    char platform[STASIS_NAME_MAX] = {0};

                    strcpy(platform, ctx->system.platform[DELIVERY_PLATFORM]);
                    if (strstr(platform, "Darwin")) {
                        memset(platform, 0, sizeof(platform));
                        strcpy(platform, "osx");
                    }
                    tolower_s(platform);
                    if (strstr(ctx->system.arch, "arm64")) {
                        strcpy(arch, "arm64");
                    } else if (strstr(ctx->system.arch, "64")) {
                        strcpy(arch, "64");
                    } else {
                        strcat(arch, "32"); // blind guess
                    }
                    tolower_s(arch);

                    sprintf(command, "mambabuild --python=%s -m ../.ci_support/%s_%s_.yaml .",
                            ctx->meta.python, platform, arch);
                } else {
                    sprintf(command, "mambabuild --python=%s .", ctx->meta.python);
                }
                status = conda_exec(command);
                if (status) {
                    guard_free(recipe_dir);
                    return -1;
                }

                if (RECIPE_TYPE_GENERIC != recipe_type) {
                    popd();
                }
                popd();
            } else {
                fprintf(stderr, "Unable to enter recipe directory %s: %s\n", recipe_dir, strerror(errno));
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
                    sprintf(cmd, "git tag -d %s", tag);
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

struct StrList *delivery_build_wheels(struct Delivery *ctx) {
    struct StrList *result = NULL;
    struct Process proc;
    memset(&proc, 0, sizeof(proc));

    result = strlist_init();
    if (!result) {
        perror("unable to allocate memory for string list");
        return NULL;
    }

    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        if (!ctx->tests[i].build_recipe && ctx->tests[i].repository) { // build from source
            char srcdir[PATH_MAX];
            char wheeldir[PATH_MAX];
            memset(srcdir, 0, sizeof(srcdir));
            memset(wheeldir, 0, sizeof(wheeldir));

            sprintf(srcdir, "%s/%s", ctx->storage.build_sources_dir, ctx->tests[i].name);
            git_clone(&proc, ctx->tests[i].repository, srcdir, ctx->tests[i].version);

            if (ctx->tests[i].repository_remove_tags && strlist_count(ctx->tests[i].repository_remove_tags)) {
                filter_repo_tags(srcdir, ctx->tests[i].repository_remove_tags);
            }

            if (!pushd(srcdir)) {
                char dname[NAME_MAX];
                char outdir[PATH_MAX];
                char cmd[PATH_MAX * 2];
                memset(dname, 0, sizeof(dname));
                memset(outdir, 0, sizeof(outdir));
                memset(cmd, 0, sizeof(outdir));

                strcpy(dname, ctx->tests[i].name);
                tolower_s(dname);
                sprintf(outdir, "%s/%s", ctx->storage.wheel_artifact_dir, dname);
                if (mkdirs(outdir, 0755)) {
                    fprintf(stderr, "failed to create output directory: %s\n", outdir);
                    guard_strlist_free(&result);
                    return NULL;
                }

                sprintf(cmd, "-m build -w -o %s", outdir);
                if (python_exec(cmd)) {
                    fprintf(stderr, "failed to generate wheel package for %s-%s\n", ctx->tests[i].name, ctx->tests[i].version);
                    guard_strlist_free(&result);
                    return NULL;
                }
                popd();
            } else {
                fprintf(stderr, "Unable to enter source directory %s: %s\n", srcdir, strerror(errno));
                guard_strlist_free(&result);
                return NULL;
            }
        }
    }
    return result;
}

