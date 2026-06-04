#include "recipe.h"

int recipe_clone(char *recipe_dir, char *url, char *gitref, char **result) {
    struct Process proc;
    char destdir[PATH_MAX];
    char *reponame = NULL;

    memset(&proc, 0, sizeof(proc));
    memset(destdir, 0, sizeof(destdir));
    reponame = path_basename(url);

    snprintf(destdir, sizeof(destdir), "%s/%s", recipe_dir, reponame);
    if (!*result) {
        *result = calloc(PATH_MAX, sizeof(*result));
        if (!*result) {
            return -1;
        }
    }
    safe_strncpy(*result, destdir, PATH_MAX);

    if (!access(destdir, F_OK)) {
        if (!strcmp(destdir, "/")) {
            SYSERROR("STASIS is misconfigured. Please check your output path(s) immediately.");
            SYSERROR("recipe_dir = '%s'\nreponame = '%s'\ndestdir = '%s'",
                    recipe_dir, reponame, destdir);
            exit(1);
        }
        if (rmtree(destdir)) {
            guard_free(*result);
            *result = NULL;
            return -1;
        }
    }
    return git_clone(&proc, url, destdir, gitref);
}

int recipe_get_build_system(const char *repopath, const int style) {
    char filename[PATH_MAX] = {0};
    safe_strncat(filename, repopath, sizeof(filename));

    if (style == RECIPE_STYLE_CONDA_FORGE) {
        safe_strncat(filename, "/recipe/recipe.yaml", sizeof(filename));
        if (!access(filename, F_OK)) {
            return RECIPE_BUILD_RATTLER;
        }
        return RECIPE_BUILD_CONDA_BUILD;
    }

    if (style == RECIPE_STYLE_ASTROCONDA || style == RECIPE_STYLE_GENERIC) {
        return RECIPE_BUILD_CONDA_BUILD;
    }

    return RECIPE_BUILD_UNKNOWN;
}

int recipe_get_style(char *repopath) {
    // conda-forge is a collection of repositories
    // "conda-forge.yml" is guaranteed to exist
    const char *marker[] = {
            "conda-forge.yml",
            "stsci",
            "meta.yaml",
            NULL
    };
    const int style[] = {
            RECIPE_STYLE_CONDA_FORGE,
            RECIPE_STYLE_ASTROCONDA,
            RECIPE_STYLE_GENERIC
    };

    for (size_t i = 0; marker[i] != NULL; i++) {
        char path[PATH_MAX] = {0};
        snprintf(path, sizeof(path), "%s/%s", repopath, marker[i]);
        int result = access(path, F_OK);
        if (!result) {
            return style[i];
        }
    }

    return RECIPE_STYLE_UNKNOWN;
}