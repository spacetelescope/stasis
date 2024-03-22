#include "wheel.h"

struct Wheel *get_wheel_file(const char *basepath, const char *name, char *to_match[]) {
    DIR *dp;
    struct dirent *rec;
    struct Wheel *result = NULL;
    char package_path[PATH_MAX];
    char package_name[NAME_MAX];

    strcpy(package_name, name);
    tolower_s(package_name);
    sprintf(package_path, "%s/%s", basepath, package_name);

    dp = opendir(package_path);
    if (!dp) {
        return NULL;
    }

    while ((rec = readdir(dp)) != NULL) {
        if (!strcmp(rec->d_name, ".") || !strcmp(rec->d_name, "..")) {
            continue;
        }
        char filename[NAME_MAX];
        strcpy(filename, rec->d_name);
        char *ext = strstr(filename, ".whl");
        if (ext) {
            *ext = '\0';
        } else {
            // not a wheel file. nothing to do
            continue;
        }

        size_t match = 0;
        size_t pattern_count = 0;
        for (; to_match[pattern_count] != NULL; pattern_count++) {
            if (strstr(filename, to_match[pattern_count])) {
                match++;
            }
        }

        if (!startswith(rec->d_name, name) || match != pattern_count) {
            continue;
        }

        result = calloc(1, sizeof(*result));
        result->path_name = realpath(package_path, NULL);
        result->file_name = strdup(rec->d_name);

        size_t parts_total;
        char **parts = split(filename, "-", 0);
        for (parts_total = 0; parts[parts_total] != NULL; parts_total++);
        if (parts_total < 6) {
            // no build tag
            result->distribution = strdup(parts[0]);
            result->version = strdup(parts[1]);
            result->build_tag = NULL;
            result->python_tag = strdup(parts[2]);
            result->abi_tag = strdup(parts[3]);
            result->platform_tag = strdup(parts[4]);
        } else {
            // has build tag
            result->distribution = strdup(parts[0]);
            result->version = strdup(parts[1]);
            result->build_tag = strdup(parts[2]);
            result->python_tag = strdup(parts[3]);
            result->abi_tag = strdup(parts[4]);
            result->platform_tag = strdup(parts[5]);
        }
        GENERIC_ARRAY_FREE(parts);
        break;
    }
    closedir(dp);
    return result;
}
