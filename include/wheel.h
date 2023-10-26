#ifndef OHMYCAL_WHEEL_H
#define OHMYCAL_WHEEL_H

#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include "str.h"

struct Wheel {
    char *distribution;
    char *version;
    char *build_tag;
    char *python_tag;
    char *abi_tag;
    char *platform_tag;
    char *path_name;
    char *file_name;
};

struct Wheel *get_wheel_file(const char *basepath, const char *name, char *to_match[]);
#endif //OHMYCAL_WHEEL_H
