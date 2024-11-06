#include "recipe.h"

int recipe_clone(char *recipe_dir, char *url, char *gitref, char **result) {
    struct Process proc;
    char destdir[PATH_MAX];
    char *reponame = NULL;

    memset(&proc, 0, sizeof(proc));
    memset(destdir, 0, sizeof(destdir));
    reponame = path_basename(url);

    sprintf(destdir, "%s/%s", recipe_dir, reponame);
    if (!*result) {
        *result = calloc(PATH_MAX, sizeof(*result));
        if (!*result) {
            return -1;
        }
    }
    strncpy(*result, destdir, PATH_MAX);

    if (!access(destdir, F_OK)) {
        if (!strcmp(destdir, "/")) {
            fprintf(stderr, "STASIS is misconfigured. Please check your output path(s) immediately.\n");
            fprintf(stderr, "recipe_dir = '%s'\nreponame = '%s'\ndestdir = '%s'\n",
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


int recipe_get_type(char *repopath) {
    // conda-forge is a collection of repositories
    // "conda-forge.yml" is guaranteed to exist
    const char *marker[] = {
            "conda-forge.yml",
            "stsci",
            "meta.yaml",
            NULL
    };
    const int type[] = {
            RECIPE_TYPE_CONDA_FORGE,
            RECIPE_TYPE_ASTROCONDA,
            RECIPE_TYPE_GENERIC
    };

    for (size_t i = 0; marker[i] != NULL; i++) {
        char path[PATH_MAX] = {0};
        sprintf(path, "%s/%s", repopath, marker[i]);
        int result = access(path, F_OK);
        if (!result) {
            return type[i];
        }
    }

    return RECIPE_TYPE_UNKNOWN;
}