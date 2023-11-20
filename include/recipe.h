//
// Created by jhunk on 10/7/23.
//

#ifndef OMC_RECIPE_H
#define OMC_RECIPE_H

#include "str.h"
#include "utils.h"

#define RECIPE_DIR "recipes"
#define RECIPE_TYPE_UNKNOWN 0
#define RECIPE_TYPE_CONDA_FORGE 1
#define RECIPE_TYPE_ASTROCONDA 2
#define RECIPE_TYPE_GENERIC 3

int recipe_clone(char *recipe_dir, char *url, char *gitref, char **result);
int recipe_get_type(char *repopath);

#endif //OMC_RECIPE_H
