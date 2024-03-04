//! @file recipe.h
#ifndef OMC_RECIPE_H
#define OMC_RECIPE_H

#include "str.h"
#include "utils.h"

//! Unable to determine recipe repo type
#define RECIPE_TYPE_UNKNOWN 0
//! Recipe repo is from conda-forge
#define RECIPE_TYPE_CONDA_FORGE 1
//! Recipe repo is from astroconda
#define RECIPE_TYPE_ASTROCONDA 2
//! Recipe repo provides the required build configurations but doesn't match conda-forge or astroconda's signature
#define RECIPE_TYPE_GENERIC 3

/**
 * Download a Conda package recipe
 *
 * ```c
 * char *recipe = NULL;
 *
 * if (recipe_clone("base/dir", "https://github.com/example/repo", "branch", &recipe)) {
 *     fprintf(stderr, "Failed to clone conda recipe\n");
 *     exit(1);
 * } else {
 *     chdir(recipe);
 * }
 * ```
 *
 * @param recipe_dir path to store repository
 * @param url remote address of git repository
 * @param gitref branch/tag/commit
 * @param result absolute path to downloaded repository
 * @return exit code from "git", -1 on error
 */
int recipe_clone(char *recipe_dir, char *url, char *gitref, char **result);

/**
 * Determine the layout/type of repository path
 *
 * ```c
 * if (recipe_clone("base/dir", "https://github.com/example/repo", "branch", &recipe)) {
 *     fprintf(stderr, "Failed to clone conda recipe\n");
 *     exit(1);
 * }
 *
 * int recipe_type;
 * recipe_type = recipe_get_type(recipe);
 * switch (recipe_type) {
 *     case RECIPE_TYPE_CONDA_FORGE:
 *         // do something specific for conda-forge directory structure
 *         break;
 *     case RECIPE_TYPE_ASTROCONDA:
 *         // do something specific for astroconda directory structure
 *         break;
 *     case RECIPE_TYPE_GENERIC:
 *         // do something specific for a directory containing a meta.yaml config
 *         break;
 *     case RECIPE_TYPE_UNKNOWN:
 *     default:
 *         // the structure is foreign or the path doesn't contain a conda recipe
 *         break;
 * }
 * ```
 *
 * @param repopath path to git repository containing conda recipe(s)
 * @return One of RECIPE_TYPE_UNKNOWN, RECIPE_TYPE_CONDA_FORGE, RECIPE_TYPE_ASTROCONDA, RECIPE_TYPE_GENERIC
 */
int recipe_get_type(char *repopath);

#endif //OMC_RECIPE_H
