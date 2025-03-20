#include "wheel.h"

struct Wheel *get_wheel_info(const char *basepath, const char *name, char *to_match[], unsigned match_mode) {
    struct dirent *rec;
    struct Wheel *result = NULL;
    char package_path[PATH_MAX];
    char package_name[NAME_MAX];

    strcpy(package_name, name);
    tolower_s(package_name);
    sprintf(package_path, "%s/%s", basepath, package_name);

    DIR *dp = opendir(package_path);
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

        if (!startswith(rec->d_name, name)) {
            continue;
        }

        if (match_mode == WHEEL_MATCH_EXACT && match != pattern_count) {
            continue;
        }

        result = calloc(1, sizeof(*result));
        if (!result) {
            SYSERROR("Unable to allocate %zu bytes for wheel struct", sizeof(*result));
            closedir(dp);
            return NULL;
        }

        result->path_name = realpath(package_path, NULL);
        if (!result->path_name) {
            SYSERROR("Unable to resolve absolute path to %s: %s", filename, strerror(errno));
            wheel_free(&result);
            closedir(dp);
            return NULL;
        }
        result->file_name = strdup(rec->d_name);
        if (!result->file_name) {
            SYSERROR("Unable to allocate bytes for %s: %s", rec->d_name, strerror(errno));
            wheel_free(&result);
            closedir(dp);
            return NULL;
        }

        size_t parts_total;
        char **parts = split(filename, "-", 0);
        if (!parts) {
            // This shouldn't happen unless a wheel file is present in the
            // directory with a malformed file name, or we've managed to
            // exhaust the system's memory
            SYSERROR("%s has no '-' separators! (Delete this file and try again)", filename);
            wheel_free(&result);
            closedir(dp);
            return NULL;
        }

        for (parts_total = 0; parts[parts_total] != NULL; parts_total++) {}
        if (parts_total == 5) {
            // no build tag
            result->distribution = strdup(parts[0]);
            result->version = strdup(parts[1]);
            result->build_tag = NULL;
            result->python_tag = strdup(parts[2]);
            result->abi_tag = strdup(parts[3]);
            result->platform_tag = strdup(parts[4]);
        } else if (parts_total == 6) {
            // has build tag
            result->distribution = strdup(parts[0]);
            result->version = strdup(parts[1]);
            result->build_tag = strdup(parts[2]);
            result->python_tag = strdup(parts[3]);
            result->abi_tag = strdup(parts[4]);
            result->platform_tag = strdup(parts[5]);
        } else {
            SYSERROR("Unknown wheel name format: %s. Expected 5 or 6 strings "
                     "separated by '-', but got %zu instead", filename, parts_total);
            guard_array_free(parts);
            wheel_free(&result);
            closedir(dp);
            return NULL;
        }
        guard_array_free(parts);
        break;
    }
    closedir(dp);
    return result;
}

void wheel_free(struct Wheel **wheel) {
    struct Wheel *w = (*wheel);
    guard_free(w->path_name);
    guard_free(w->file_name);
    guard_free(w->distribution);
    guard_free(w->version);
    guard_free(w->build_tag);
    guard_free(w->python_tag);
    guard_free(w->abi_tag);
    guard_free(w->python_tag);
    guard_free(w->platform_tag);
    guard_free(w);
}
