#include "wheel.h"

#include <ctype.h>

#include "str.h"
#include "strlist.h"

const char *WHEEL_META_KEY[] = {
    "Metadata-Version",
    "Name",
    "Version",
    "Author",
    "Author-email",
    "Maintainer",
    "Maintainer-email",
    "Summary",
    "License",
    "License-Expression",
    "License-File",
    "Home-page",
    "Download-URL",
    "Project-URL",
    "Classifier",
    "Requires-Python",
    "Requires-External",
    "Import-Name",
    "Import-Namespace",
    "Requires-Dist",
    "Provides",
    "Provides-Dist",
    "Provides-Extra",
    "Obsoletes",
    "Obsoletes-Dist",
    "Platform",
    "Supported-Platform",
    "Keywords",
    "Dynamic",
    "Description-Content-Type",
    "Description",
};

const char *WHEEL_DIST_KEY[] = {
    "Wheel-Version",
    "Generator",
    "Root-Is-Purelib",
    "Tag",
    "Zip-Safe",
    "Top-Level",
    "Entry-points",
    "Record",
};

static ssize_t wheel_parse_wheel(struct Wheel * pkg, const char * data) {
    int read_as = 0;
    struct StrList *lines = strlist_init();
    if (!lines) {
        return -1;
    }
    strlist_append_tokenize(lines, (char *) data, "\r\n");

    for (size_t i = 0; i < strlist_count(lines); i++) {
        char *line = strlist_item(lines, i);
        if (isempty(line)) {
            continue;
        }

        char **pair = split(line, ":", 1);
        if (pair) {
            char *key = strdup(strip(pair[0]));
            char *value = strdup(lstrip(pair[1]));

            if (!key || !value) {
                return -1;
            }

            if (!strcasecmp(key, WHEEL_DIST_KEY[WHEEL_DIST_VERSION])) {
                read_as = WHEEL_DIST_VERSION;
            } else if (!strcasecmp(key, WHEEL_DIST_KEY[WHEEL_DIST_GENERATOR])) {
                read_as = WHEEL_DIST_GENERATOR;
            } else if (!strcasecmp(key, WHEEL_DIST_KEY[WHEEL_DIST_ROOT_IS_PURELIB])) {
                read_as = WHEEL_DIST_ROOT_IS_PURELIB;
            } else if (!strcasecmp(key, WHEEL_DIST_KEY[WHEEL_DIST_TAG])) {
                read_as = WHEEL_DIST_TAG;
            }

            switch (read_as) {
                case WHEEL_DIST_VERSION: {
                    pkg->wheel_version = strdup(value);
                    if (!pkg->wheel_version) {
                        // memory error
                        return -1;
                    }
                    break;
                }
                case WHEEL_DIST_GENERATOR: {
                    pkg->generator = strdup(value);
                    if (!pkg->generator) {
                        // memory error
                        return -1;
                    }
                    break;
                }
                case WHEEL_DIST_ROOT_IS_PURELIB: {
                    pkg->root_is_pure_lib = strdup(value);
                    if (!pkg->root_is_pure_lib) {
                        // memory error
                        return -1;
                    }
                    break;
                }
                case WHEEL_DIST_TAG: {
                    if (!pkg->tag) {
                        pkg->tag = strlist_init();
                        if (!pkg->tag) {
                            return -1;
                        }
                    }
                    strlist_append(&pkg->tag, value);
                    break;
                }
                default:
                    fprintf(stderr, "warning: unhandled wheel key on line %zu:\nbuffer contents: '%s'\n", i, value);
                    break;
            }
            guard_free(key);
            guard_free(value);
        }
        guard_array_free(pair);
    }
    guard_strlist_free(&lines);
    return data ? (ssize_t) strlen(data) : -1;
}


static inline int is_continuation(const char *s) {
    return s && (s[0] == ' ' || s[0] == '\t');
}

static ssize_t wheel_parse_metadata(struct WheelMetadata * const pkg, const char * const data) {
    int read_as = WHEEL_KEY_UNKNOWN;
    // triggers
    int reading_multiline = 0;
    int reading_extra = 0;
    size_t provides_extra_i = 0;
    int reading_description = 0;
    size_t base_description_len = 1024;
    size_t len_description = 0;
    struct WheelMetadata_ProvidesExtra *current_extra = NULL;

    if (!data) {
        // data can't be NULL
        return -1;
    }

    pkg->provides_extra = calloc(WHEEL_MAXELEM + 1, sizeof(pkg->provides_extra[0]));
    if (!pkg->provides_extra) {
        // memory error
        return -1;
    }

    struct StrList *lines = strlist_init();
    if (!lines) {
        // memory error
        return -1;
    }

    strlist_append_tokenize_raw(lines, (char *) data, "\r\n");
    for (size_t i = 0; i < strlist_count(lines); i++) {
        const char *line = strlist_item(lines, i);
        char *key = NULL;
        char *value = NULL;

        reading_multiline = is_continuation(line);
        if (!reading_multiline && line[0] == '\0') {
            reading_description = 1;
            read_as = WHEEL_META_DESCRIPTION;
        }

        char **pair = split((char *) line, ":", 1);
        if (!pair) {
            // memory error
            return -1;
        }

        if (!reading_description && !reading_multiline && pair[1]) {
            key = strip(strdup(pair[0]));
            value = lstrip(strdup(pair[1]));

            if (!key || !value) {
                // memory error
                return -1;
            }

            if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_METADATA_VERSION])) {
                read_as = WHEEL_META_METADATA_VERSION;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_NAME])) {
                read_as = WHEEL_META_NAME;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_VERSION])) {
                read_as = WHEEL_META_VERSION;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_SUMMARY])) {
                read_as = WHEEL_META_SUMMARY;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_AUTHOR])) {
                read_as = WHEEL_META_AUTHOR;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_AUTHOR_EMAIL])) {
                read_as = WHEEL_META_AUTHOR_EMAIL;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_MAINTAINER])) {
                read_as = WHEEL_META_MAINTAINER;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_MAINTAINER_EMAIL])) {
                read_as = WHEEL_META_MAINTAINER_EMAIL;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_LICENSE])) {
                read_as = WHEEL_META_LICENSE;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_LICENSE_EXPRESSION])) {
                read_as = WHEEL_META_LICENSE_EXPRESSION;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_HOME_PAGE])) {
                read_as = WHEEL_META_HOME_PAGE;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_DOWNLOAD_URL])) {
                read_as = WHEEL_META_DOWNLOAD_URL;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_PROJECT_URL])) {
                read_as = WHEEL_META_PROJECT_URL;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_CLASSIFIER])) {
                read_as = WHEEL_META_CLASSIFIER;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_REQUIRES_PYTHON])) {
                read_as = WHEEL_META_REQUIRES_PYTHON;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_REQUIRES_EXTERNAL])) {
                read_as = WHEEL_META_REQUIRES_EXTERNAL;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_DESCRIPTION_CONTENT_TYPE])) {
                read_as = WHEEL_META_DESCRIPTION_CONTENT_TYPE;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_LICENSE_FILE])) {
                read_as = WHEEL_META_LICENSE_FILE;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_REQUIRES_DIST])) {
                read_as = WHEEL_META_REQUIRES_DIST;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_PROVIDES])) {
                read_as = WHEEL_META_PROVIDES;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_IMPORT_NAME])) {
                read_as = WHEEL_META_IMPORT_NAME;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_IMPORT_NAMESPACE])) {
                read_as = WHEEL_META_IMPORT_NAMESPACE;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_PROVIDES_DIST])) {
                read_as = WHEEL_META_PROVIDES_DIST;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_PROVIDES_EXTRA])) {
                read_as = WHEEL_META_PROVIDES_EXTRA;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_PLATFORM])) {
                read_as = WHEEL_META_PLATFORM;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_SUPPORTED_PLATFORM])) {
                read_as = WHEEL_META_SUPPORTED_PLATFORM;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_KEYWORDS])) {
                read_as = WHEEL_META_KEYWORDS;
            } else if (!strcasecmp(key,WHEEL_META_KEY[WHEEL_META_DYNAMIC])) {
                read_as = WHEEL_META_DYNAMIC;
            } else {
                read_as = WHEEL_KEY_UNKNOWN;
            }
        } else {
            value = strdup(line);
            if (!value) {
                // memory error
                return -1;
            }
        }

        switch (read_as) {
            case WHEEL_META_METADATA_VERSION: {
                pkg->metadata_version = strdup(value);
                if (!pkg->metadata_version) {
                    // memory error
                    return -1;
                }
                break;
            }
            case WHEEL_META_NAME: {
                pkg->name = strdup(value);
                if (!pkg->name) {
                    // memory error
                    return -1;
                }
                break;
            }
            case WHEEL_META_VERSION: {
                pkg->version = strdup(value);
                if (!pkg->version) {
                    // memory error
                    return -1;
                }
                break;
            }
            case WHEEL_META_SUMMARY: {
                pkg->summary = strdup(value);
                if (!pkg->summary) {
                    // memory error
                    return -1;
                }
                break;
            }
            case WHEEL_META_AUTHOR: {
                if (!pkg->author) {
                    pkg->author = strlist_init();
                    if (!pkg->author) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append_tokenize(pkg->author, value, ",");
                break;
            }
            case WHEEL_META_AUTHOR_EMAIL: {
                if (!pkg->author_email) {
                    pkg->author_email = strlist_init();
                    if (!pkg->author_email) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append_tokenize(pkg->author_email, value, ",");
                break;
            }
            case WHEEL_META_MAINTAINER: {
                if (!pkg->maintainer) {
                    pkg->maintainer = strlist_init();
                    if (!pkg->maintainer) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append_tokenize(pkg->maintainer, value, ",");
                break;
            }
            case WHEEL_META_MAINTAINER_EMAIL: {
                if (!pkg->maintainer_email) {
                    pkg->maintainer_email = strlist_init();
                    if (!pkg->maintainer_email) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append_tokenize(pkg->maintainer_email, value, ",");
                break;
            }
            case WHEEL_META_LICENSE: {
                if (!reading_multiline) {
                    pkg->license = strdup(value);
                    if (!pkg->license) {
                        // memory error
                        return -1;
                    }
                } else {
                    if (pkg->license) {
                        consume_append(&pkg->license, line, "\r\n");
                    } else {
                        // previously unhandled memory error
                        return -1;
                    }
                }
                break;
            }
            case WHEEL_META_LICENSE_EXPRESSION: {
                pkg->license_expression = strdup(value);
                if (!pkg->license_expression) {
                    // memory error
                    return -1;
                }
                break;
            }
            case WHEEL_META_HOME_PAGE: {
                pkg->home_page = strdup(value);
                if (!pkg->home_page) {
                    // memory error
                    return -1;
                }
                break;
            }
            case WHEEL_META_DOWNLOAD_URL:
                pkg->download_url = strdup(value);
                if (!pkg->download_url) {
                    // memory error
                    return -1;
                }
                break;
            case WHEEL_META_PROJECT_URL: {
                if (!pkg->project_url) {
                    pkg->project_url = strlist_init();
                    if (!pkg->project_url) {
                        // memory_error
                        return -1;
                    }
                }
                strlist_append(&pkg->project_url, value);
                break;
            }
            case WHEEL_META_CLASSIFIER: {
                if (!pkg->classifier) {
                    pkg->classifier = strlist_init();
                    if (!pkg->classifier) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append(&pkg->classifier, value);
                break;
            }
            case WHEEL_META_REQUIRES_PYTHON: {
                if (!pkg->requires_python) {
                    pkg->requires_python = strlist_init();
                    if (!pkg->requires_python) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append(&pkg->requires_python, value);
                break;
            }
            case WHEEL_META_REQUIRES_EXTERNAL: {
                if (!pkg->requires_external) {
                    pkg->requires_external = strlist_init();
                    if (!pkg->requires_external) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append(&pkg->requires_external, value);
                break;
            }
            case WHEEL_META_DESCRIPTION_CONTENT_TYPE: {
                pkg->description_content_type = strdup(value);
                if (!pkg->description_content_type) {
                    // memory error
                    return -1;
                }
                break;
            }
            case WHEEL_META_LICENSE_FILE: {
                if (!pkg->license_file) {
                    pkg->license_file = strlist_init();
                    if (!pkg->license_file) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append(&pkg->license_file, value);
                break;
            }
            case WHEEL_META_IMPORT_NAME: {
                if (!pkg->import_name) {
                    pkg->import_name = strlist_init();
                    if (!pkg->import_name) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append(&pkg->import_name, value);
                break;
            }
            case WHEEL_META_IMPORT_NAMESPACE: {
                if (!pkg->import_namespace) {
                    pkg->import_namespace = strlist_init();
                    if (!pkg->import_namespace) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append(&pkg->import_namespace, value);
                break;
            }
            case WHEEL_META_REQUIRES_DIST: {
                if (!pkg->requires_dist) {
                    pkg->requires_dist = strlist_init();
                    if (!pkg->requires_dist) {
                        // memory error
                        return -1;
                    }
                }
                if (reading_extra) {
                    if (strrchr(value, ';')) {
                        *strrchr(value, ';') = 0;
                    }
                    if (!current_extra) {
                        reading_extra = 0;
                        break;
                    }
                    if (!current_extra->requires_dist) {
                        current_extra->requires_dist = strlist_init();
                        if (!current_extra->requires_dist) {
                            // memory error
                            return -1;
                        }
                    }
                    strlist_append(&current_extra->requires_dist, value);
                } else {
                    strlist_append(&pkg->requires_dist, value);
                    reading_extra = 0;
                }
                break;
            }
            case WHEEL_META_PROVIDES: {
                if (!pkg->provides) {
                    pkg->provides = strlist_init();
                    if (!pkg->provides) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append(&pkg->provides, value);
                break;
            }
            case WHEEL_META_PROVIDES_DIST: {
                if (!pkg->provides_dist) {
                    pkg->provides_dist = strlist_init();
                    if (!pkg->provides_dist) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append(&pkg->provides_dist, value);
                break;
            }
            case WHEEL_META_PROVIDES_EXTRA:
                pkg->provides_extra[provides_extra_i] = calloc(1, sizeof(*pkg->provides_extra[0]));
                if (!pkg->provides_extra[provides_extra_i]) {
                    // memory error
                    return -1;
                }
                current_extra = pkg->provides_extra[provides_extra_i];
                current_extra->target = strdup(value);
                if (!current_extra->target) {
                    // memory error
                    return -1;
                }
                reading_extra = 1;
                provides_extra_i++;
                break;
            case WHEEL_META_PLATFORM:{
                if (!pkg->platform) {
                    pkg->platform = strlist_init();
                    if (!pkg->platform) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append(&pkg->platform, value);
                break;
            }
            case WHEEL_META_SUPPORTED_PLATFORM: {
                if (!pkg->supported_platform) {
                    pkg->supported_platform = strlist_init();
                    if (!pkg->supported_platform) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append(&pkg->supported_platform, value);
                break;
            }
            case WHEEL_META_KEYWORDS: {
                if (!pkg->keywords) {
                    pkg->keywords = strlist_init();
                    if (!pkg->keywords) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append_tokenize(pkg->keywords, value, ",");
                break;
            }
            case WHEEL_META_DYNAMIC: {
                if (!pkg->dynamic) {
                    pkg->dynamic = strlist_init();
                    if (!pkg->dynamic) {
                        // memory error
                        return -1;
                    }
                }
                strlist_append(&pkg->dynamic, value);
                break;
            }
            case WHEEL_META_DESCRIPTION: {
                // reading_description will never be reset to zero
                reading_description = 1;
                if (!pkg->description) {
                    pkg->description = malloc(base_description_len + 1);
                    if (!pkg->description) {
                        return -1;
                    }
                    len_description = snprintf(pkg->description, base_description_len, "%s\n", line);
                } else {
                    const size_t next_len = snprintf(NULL, 0, "%s\n%s\n", pkg->description, line);
                    if (next_len + 1 > base_description_len) {
                        base_description_len *= 2;
                        char *tmp = realloc(pkg->description, base_description_len + 1);
                        if (!tmp) {
                            // memory error
                            guard_free(pkg->description);
                            return -1;
                        }
                        pkg->description = tmp;
                    }
                    len_description += snprintf(pkg->description + len_description, next_len + 1, "%s\n", line);

                    //consume_append(&pkg->description, line, "\r\n");
                }
                break;
            }
            case WHEEL_KEY_UNKNOWN:
            default:
                fprintf(stderr, "warning: unhandled metadata key on line %zu:\nbuffer contents: '%s'\n", i, value);
                break;
        }
        guard_free(key);
        guard_free(value);
        guard_array_free(pair);
        if (reading_multiline) {
            guard_free(value);
        }
    }
    guard_strlist_free(&lines);

    return 0;
}

int wheel_get_file_contents(const char *wheelfile, const char *filename, char **contents) {
    int status = 0;
    int err = 0;
    struct zip_stat archive_info;
    zip_t *archive = zip_open(wheelfile, 0, &err);
    if (!archive) {
        return status;
    }

    zip_stat_init(&archive_info);
    for (size_t i = 0; zip_stat_index(archive, i, 0, &archive_info) >= 0; i++) {
        char internal_path[1024] = {0};
        snprintf(internal_path, sizeof(internal_path), "%s", filename);
        const int match = fnmatch(internal_path, archive_info.name, 0);
        if (match == FNM_NOMATCH) {
            continue;
        }
        if (match < 0) {
            goto GWM_FAIL;
        }

        zip_file_t *handle = zip_fopen_index(archive, i, 0);
        if (!handle) {
            goto GWM_FAIL;
        }

        *contents = calloc(archive_info.size + 1, sizeof(**contents));
        if (!*contents) {
            zip_fclose(handle);
            goto GWM_FAIL;
        }

        if (zip_fread(handle, *contents, archive_info.size) < 0) {
            zip_fclose(handle);
            guard_free(*contents);
            goto GWM_FAIL;
        }
        zip_fclose(handle);
        break;
    }

    goto GWM_END;
    GWM_FAIL:
    status = -1;

    GWM_END:
    zip_close(archive);
    return status;
}

static int wheel_metadata_get(const struct Wheel *pkg, const char *wheel_filename) {
    char *data = NULL;
    if (wheel_get_file_contents(wheel_filename, "*.dist-info/METADATA", &data)) {
        return -1;
    }
    const ssize_t result = wheel_parse_metadata(pkg->metadata, data);
    char *data_orig = data;
    guard_free(data_orig);
    return (int) result;
}

static struct WheelValue wheel_data_dump(const struct Wheel *pkg, const ssize_t key) {
    struct WheelValue result;
    result.type = WHEELVAL_STR;
    result.count = 0;
    switch (key) {
        case WHEEL_DIST_VERSION:
            result.data = pkg->wheel_version;
            break;
        case WHEEL_DIST_GENERATOR:
            result.data = pkg->generator;
            break;
        case WHEEL_DIST_ZIP_SAFE:
            result.data = pkg->zip_safe ? "True" : "False";
            break;
        case WHEEL_DIST_ROOT_IS_PURELIB:
            result.data = pkg->root_is_pure_lib ? "True" : "False";
            break;
        case WHEEL_DIST_TOP_LEVEL:
            result.type = WHEELVAL_STRLIST;
            result.data = pkg->top_level;
            result.count = strlist_count(pkg->top_level);
            break;
        case WHEEL_DIST_TAG:
            result.type = WHEELVAL_STRLIST;
            result.data = pkg->tag;
            result.count = strlist_count(pkg->tag);
            break;
        case WHEEL_DIST_RECORD:
            result.type = WHEELVAL_OBJ_RECORD;
            result.data = pkg->record;
            result.count = pkg->num_record;
            break;
        case WHEEL_DIST_ENTRY_POINT:
            result.type = WHEELVAL_OBJ_ENTRY_POINT;
            result.data = pkg->entry_point;
            result.count = pkg->num_entry_point;
            break;
        default:
            result.data = NULL;
            result.type = WHEEL_KEY_UNKNOWN;
            break;
    }

    return result;
}

static struct WheelValue wheel_metadata_dump(const struct Wheel *pkg, const ssize_t key) {
    const struct WheelMetadata *meta = pkg->metadata;
    struct WheelValue result;
    result.type = WHEELVAL_STR;
    result.count = 0;
    switch (key) {
        case WHEEL_META_METADATA_VERSION:
            result.data = meta->metadata_version;
            break;
        case WHEEL_META_NAME:
            result.data = meta->name;
            break;
        case WHEEL_META_VERSION:
            result.data = meta->version;
            break;
        case WHEEL_META_SUMMARY:
            result.data = meta->summary;
            break;
        case WHEEL_META_AUTHOR:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->author;
            break;
        case WHEEL_META_AUTHOR_EMAIL:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->author_email;
            break;
        case WHEEL_META_MAINTAINER:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->maintainer;
            break;
        case WHEEL_META_MAINTAINER_EMAIL:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->maintainer_email;
            break;
        case WHEEL_META_LICENSE:
            result.data = meta->license;
            break;
        case WHEEL_META_HOME_PAGE:
            result.data = meta->home_page;
            break;
        case WHEEL_META_DOWNLOAD_URL:
            result.data = meta->download_url;
            break;
        case WHEEL_META_PROJECT_URL:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->project_url;
            result.count = strlist_count(meta->project_url);
            break;
        case WHEEL_META_CLASSIFIER:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->classifier;
            result.count = strlist_count(meta->classifier);
            break;
        case WHEEL_META_REQUIRES_PYTHON:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->requires_python;
            result.count = strlist_count(meta->requires_python);
            break;
        case WHEEL_META_DESCRIPTION_CONTENT_TYPE:
            result.data = meta->description_content_type;
            break;
        case WHEEL_META_LICENSE_FILE:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->license_file;
            result.count = strlist_count(meta->license_file);
            break;
        case WHEEL_META_LICENSE_EXPRESSION:
            result.data = meta->license_expression;
            break;
        case WHEEL_META_IMPORT_NAME:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->import_name;
            result.count = strlist_count(meta->import_name);
            break;
        case WHEEL_META_IMPORT_NAMESPACE:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->import_namespace;
            result.count = strlist_count(meta->import_namespace);
            break;
        case WHEEL_META_REQUIRES_DIST:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->requires_dist;
            result.count = strlist_count(meta->requires_dist);
            break;
        case WHEEL_META_PROVIDES_DIST:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->provides_dist;
            result.count = strlist_count(meta->provides_dist);
            break;
        case WHEEL_META_PROVIDES_EXTRA:
            result.type = WHEELVAL_OBJ_EXTRA;
            result.data = (void *) meta->provides_extra;
            break;
        case WHEEL_META_OBSOLETES:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->obsoletes;
            result.count = strlist_count(meta->obsoletes);
            break;
        case WHEEL_META_OBSOLETES_DIST:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->obsoletes_dist;
            result.count = strlist_count(meta->obsoletes_dist);
            break;
        case WHEEL_META_DESCRIPTION:
            result.type = WHEELVAL_STR;
            result.data = meta->description;
            break;
        case WHEEL_META_PLATFORM:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->platform;
            result.count = strlist_count(meta->platform);
            break;
        case WHEEL_META_SUPPORTED_PLATFORM:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->supported_platform;
            result.count = strlist_count(meta->supported_platform);
            break;
        case WHEEL_META_KEYWORDS:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->keywords;
            result.count = strlist_count(meta->keywords);
            break;
        case WHEEL_META_DYNAMIC:
            result.type = WHEELVAL_STRLIST;
            result.data = meta->dynamic;
            result.count = strlist_count(meta->dynamic);
            break;
        case WHEEL_KEY_UNKNOWN:
        default:
            result.data = NULL;
            break;
    }
    return result;
}

static ssize_t get_key_index(const char **arr, const char *key) {
    for (ssize_t i = 0; arr[i] != NULL; i++) {
        if (strcmp(arr[i], key) == 0) {
            return i;
        }
    }
    return -1;
}

const char *wheel_get_key_by_id(const int from, const ssize_t id) {
    if (from == WHEEL_FROM_DIST) {
        if (id >= 0 && id < WHEEL_DIST_END_ENUM) {
            return WHEEL_DIST_KEY[id];
        }
    }
    if (from == WHEEL_FROM_METADATA) {
        if (id >= 0 && id < WHEEL_META_END_ENUM) {
            return WHEEL_META_KEY[id];
        }
    }
    return NULL;
}

struct WheelValue wheel_get_value_by_name(const struct Wheel *pkg, const int from, const char *key) {
    struct WheelValue result = {0};
    ssize_t id;

    if (from == WHEEL_FROM_DIST) {
        id = get_key_index(WHEEL_DIST_KEY, key);
        result = wheel_data_dump(pkg, id);
    } else if (from == WHEEL_FROM_METADATA) {
        id = get_key_index(WHEEL_META_KEY, key);
        result = wheel_metadata_dump(pkg, id);
    } else {
        result.data = NULL;
        result.type = WHEEL_KEY_UNKNOWN;
    }

    return result;
}

struct WheelValue wheel_get_value_by_id(const struct Wheel *pkg, const int from, const ssize_t id) {
    struct WheelValue result = {0};

    if (from == WHEEL_FROM_DIST) {
        result = wheel_data_dump(pkg, id);
    } else if (from == WHEEL_FROM_METADATA) {
        result = wheel_metadata_dump(pkg, id);
    } else {
        result.data = NULL;
        result.type = -1;
    }

    return result;
}

void wheel_record_free(struct WheelRecord **record) {
    guard_free((*record)->filename);
    guard_free((*record)->checksum);
    guard_free(*record);
}

void wheel_entry_point_free(struct WheelEntryPoint **entry) {
    guard_free((*entry)->name);
    guard_free((*entry)->function);
    guard_free((*entry)->type);
    guard_free(*entry);
}

void wheel_metadata_free(struct WheelMetadata *meta) {
    guard_free(meta->license);
    guard_free(meta->license_expression);
    guard_free(meta->version);
    guard_free(meta->name);
    guard_free(meta->description);
    guard_free(meta->metadata_version);
    guard_free(meta->summary);
    guard_free(meta->description_content_type);
    guard_free(meta->home_page);
    guard_free(meta->download_url);

    guard_strlist_free(&meta->author_email);
    guard_strlist_free(&meta->author);
    guard_strlist_free(&meta->maintainer);
    guard_strlist_free(&meta->maintainer_email);
    guard_strlist_free(&meta->requires_python);
    guard_strlist_free(&meta->requires_external);
    guard_strlist_free(&meta->project_url);
    guard_strlist_free(&meta->classifier);
    guard_strlist_free(&meta->requires_dist);
    guard_strlist_free(&meta->keywords);
    guard_strlist_free(&meta->license_file);

    for (size_t i = 0; meta->provides_extra[i] != NULL; i++) {
        guard_free(meta->provides_extra[i]->target);
        guard_strlist_free(&meta->provides_extra[i]->requires_dist);
        guard_free(meta->provides_extra[i]);
    }
    guard_free(meta->provides_extra);

    guard_free(meta);
}

void wheel_package_free(struct Wheel **pkg) {
    guard_free((*pkg)->wheel_version);
    guard_free((*pkg)->generator);
    guard_free((*pkg)->root_is_pure_lib);
    wheel_metadata_free((*pkg)->metadata);

    guard_strlist_free(&(*pkg)->tag);
    guard_strlist_free(&(*pkg)->top_level);
    for (size_t i = 0; (*pkg)->record && (*pkg)->record[i] != NULL; i++) {
        wheel_record_free(&(*pkg)->record[i]);
    }
    guard_free((*pkg)->record);

    for (size_t i = 0; (*pkg)->entry_point && (*pkg)->entry_point[i] != NULL; i++) {
        wheel_entry_point_free(&(*pkg)->entry_point[i]);
    }
    guard_free((*pkg)->entry_point);
    guard_free((*pkg));
}

int wheel_get_top_level(struct Wheel *pkg, const char *filename) {
    char *data = NULL;
    if (wheel_get_file_contents(filename, "*.dist-info/top_level.txt", &data)) {
        guard_free(data);
        return -1;
    }
    if (!pkg->top_level) {
        pkg->top_level = strlist_init();
    }
    strlist_append_tokenize(pkg->top_level, data, "\r\n");
    guard_free(data);
    return 0;
}

int wheel_get_zip_safe(struct Wheel *pkg, const char *filename) {
    char *data = NULL;
    const int exists = wheel_get_file_contents(filename, "*.dist-info/zip-safe", &data) == 0;
    guard_free(data);

    pkg->zip_safe = 0;
    if (exists) {
        pkg->zip_safe = 1;
    }

    return 0;
}

int wheel_get_records(struct Wheel *pkg, const char *filename) {
    char *data = NULL;
    const int exists = wheel_get_file_contents(filename, "*.dist-info/RECORD", &data) == 0;

    if (!exists) {
        guard_free(data);
        return 1;
    }

    const size_t records_initial_count = 2;
    pkg->record = calloc(records_initial_count, sizeof(*pkg->record));
    if (!pkg->record) {
        guard_free(data);
        return 1;
    }
    size_t records_count = 0;

    const char *token = NULL;
    char *data_orig = data;
    while ((token = strsep(&data, "\r\n")) != NULL) {
        if (!strlen(token)) {
            continue;
        }

        pkg->record[records_count] = calloc(1, sizeof(*pkg->record[0]));
        if (!pkg->record[records_count]) {
            return -1;
        }

        struct WheelRecord *record = pkg->record[records_count];
        for (size_t x = 0; x < 3; x++) {
            const char *next_comma = strpbrk(token, ",");
            if (next_comma) {
                if (x == 0) {
                    record->filename = strndup(token, next_comma - token);
                } else if (x == 1) {
                    record->checksum = strndup(token, next_comma - token);
                }
                token = next_comma + 1;
            } else {
                record->size = strtol(token, NULL, 10);
            }
        }
        records_count++;

        struct WheelRecord **tmp = realloc(pkg->record, (records_count + 2) * sizeof(*pkg->record));
        if (tmp == NULL) {
            guard_free(data);
            return -1;
        }
        pkg->record = tmp;
        pkg->record[records_count + 1] = NULL;
    }

    pkg->num_record = records_count;
    guard_free(data_orig);
    return 0;
}

int wheel_get(struct Wheel **pkg, const char *filename) {
    char *data = NULL;
    if (wheel_get_file_contents(filename, "*.dist-info/WHEEL", &data) < 0) {
        return -1;
    }
    const ssize_t result = wheel_parse_wheel(*pkg, data);
    guard_free(data);
    return (int) result;
}

int wheel_get_entry_point(struct Wheel *pkg, const char *filename) {
    char *data = NULL;
    if (wheel_get_file_contents(filename, "*.dist-info/entry_points.txt", &data) < 0) {
        return -1;
    }

    struct StrList *lines = strlist_init();
    if (!lines) {
        goto GEP_FAIL;
    }
    strlist_append_tokenize(lines, data, "\r\n");

    const size_t line_count = strlist_count(lines);
    size_t usable_lines = line_count;
    for (size_t i = 0; i < line_count; i++) {
        const char *item = strlist_item(lines, i);
        if (isempty((char *) item) || item[0] == '[') {
            usable_lines--;
        }
    }

    pkg->entry_point = calloc(usable_lines + 1, sizeof(*pkg->entry_point));
    if (!pkg->entry_point) {
        goto GEP_FAIL;
    }

    for (size_t i = 0; i < usable_lines; i++) {
        pkg->entry_point[i] = calloc(1, sizeof(*pkg->entry_point[0]));
        if (!pkg->entry_point[i]) {
            goto GEP_FAIL;
        }
    }

    size_t x = 0;
    char section[255] = {0};
    for (size_t i = 0; i < line_count; i++) {
        const char *item = strlist_item(lines, i);
        if (isempty((char *) item)) {
            continue;
        }

        if (strpbrk(item, "[")) {
            const size_t start = strcspn((char *) item, "[") + 1;
            if (start) {
                const size_t len = strcspn((char *) item, "]");
                strncpy(section, item + start, len - start);
                section[len - start] = '\0';
                continue;
            }
        }

        pkg->entry_point[x]->type = strdup(section);
        if (!pkg->entry_point[x]->type) {
            goto GEP_FAIL;
        }

        char **pair = split((char *) item, "=", 1);
        if (!pair) {
            wheel_entry_point_free(&pkg->entry_point[x]);
            goto GEP_FAIL;
        }

        pkg->entry_point[x]->name = strdup(strip(pair[0]));
        if (!pkg->entry_point[x]->name) {
            wheel_entry_point_free(&pkg->entry_point[x]);
            guard_array_free(pair);
            goto GEP_FAIL;
        }

        pkg->entry_point[x]->function = strdup(lstrip(pair[1]));
        if (!pkg->entry_point[x]->function) {
            wheel_entry_point_free(&pkg->entry_point[x]);
            guard_array_free(pair);
            goto GEP_FAIL;
        }
        guard_array_free(pair);
        x++;
    }
    pkg->num_entry_point = x;
    guard_strlist_free(&lines);
    guard_free(data);
    return 0;

    GEP_FAIL:
    guard_strlist_free(&lines);
    guard_free(data);
    return -1;
}

int wheel_value_error(struct WheelValue const *val) {
    if (val) {
        if (val->type < 0 && val->data == NULL) {
            return 1;
        }
    }
    return 0;
}

int wheel_show_info(const struct Wheel *wheel) {
    printf("WHEEL INFO\n\n");
    for (ssize_t i = 0; i < WHEEL_DIST_END_ENUM; i++) {
        const char *key = wheel_get_key_by_id(WHEEL_FROM_DIST, i);
        if (!key) {
            fprintf(stderr, "wheel_get_key_by_id(%zi) failed\n", i);
            return -1;
        }

        printf("%s: ", key);
        fflush(stdout);
        const struct WheelValue dist = wheel_get_value_by_id(wheel, WHEEL_FROM_DIST, i);
        if (wheel_value_error(&dist)) {
            fprintf(stderr, "wheel_get_value_by_id(%zi) failed\n", i);
            return -1;
        }
        switch (dist.type) {
            case WHEELVAL_STR: {
                char *s = dist.data;
                if (s != NULL && !isempty(s)) {
                    printf("%s\n", s);
                } else {
                    printf("[N/A]\n");
                }
                break;
            }
            case WHEELVAL_STRLIST: {
                struct StrList *list = dist.data;
                if (list) {
                    printf("\n");
                    for (size_t x = 0; x < strlist_count(list); x++) {
                        const char *item = strlist_item(list, x);
                        printf("    %s\n", item);
                    }
                } else {
                    printf("[N/A]\n");
                }
                break;
            }
            case WHEELVAL_OBJ_RECORD: {
                struct WheelRecord **record = dist.data;
                if (record && *record) {
                    printf("\n");
                    for (size_t x = 0; x < dist.count; x++) {
                        printf("    [%zu] %s (size: %zu bytes, checksum: %s)\n", x, wheel->record[x]->filename, wheel->record[x]->size, strlen(wheel->record[x]->checksum) ? wheel->record[x]->checksum : "N/A");
                    }
                } else {
                    printf("[N/A]\n");
                }
                break;
            }
            case WHEELVAL_OBJ_ENTRY_POINT: {
                struct WheelEntryPoint **entry = dist.data;
                if (entry && *entry) {
                    printf("\n");
                    for (size_t x = 0; x < dist.count; x++) {
                        printf("    [%zu] type: %s, name: %s, function: %s\n", x, entry[x]->type, entry[x]->name, entry[x]->function);
                    }
                } else {
                    printf("[N/A]\n");
                }
                break;
            }
            default:
                printf("[no handler]\n");
                break;
        }

    }

    printf("\nPACKAGE INFO\n\n");
    for (ssize_t i = 0; i < WHEEL_META_END_ENUM; i++) {
        const char *key = wheel_get_key_by_id(WHEEL_FROM_METADATA, i);
        if (!key) {
            fprintf(stderr, "wheel_get_key_by_id(%zi) failed\n", i);
            return -1;
        }
        printf("%s: ", key);
        fflush(stdout);

        const struct WheelValue pkg = wheel_get_value_by_id(wheel, WHEEL_FROM_METADATA, i);
        if (wheel_value_error(&pkg)) {
            fprintf(stderr, "wheel_get_value_by_id(%zi) failed\n", i);
            return -1;
        }
        switch (pkg.type) {
            case WHEELVAL_STR: {
                char *s = pkg.data;
                if (s != NULL && !isempty(s)) {
                    printf("%s\n", s);
                } else {
                    printf("[N/A]\n");
                }
                break;
            }
            case WHEELVAL_STRLIST: {
                struct StrList *list = pkg.data;
                if (list) {
                    printf("\n");
                    for (size_t x = 0; x < strlist_count(list); x++) {
                        const char *item = strlist_item(list, x);
                        printf("    %s\n", item);
                    }
                } else {
                    printf("[N/A]\n");
                }
                break;
            }
            case WHEELVAL_OBJ_EXTRA: {
                const struct WheelMetadata_ProvidesExtra **extra = pkg.data;
                printf("\n");
                if (*extra) {
                    for (size_t x = 0; extra[x] != NULL; x++) {
                        printf("    + %s\n", extra[x]->target);
                        for (size_t z = 0; z < strlist_count(extra[x]->requires_dist); z++) {
                            const char *item = strlist_item(extra[x]->requires_dist, z);
                            printf("     `- %s\n", item);
                        }
                    }
                } else {
                    printf("[N/A]\n");
                }
                break;
            }
            default:
                break;
        }
    }
    return 0;
}
int wheel_package(struct Wheel **pkg, const char *filename) {
    if (!filename) {
        return -1;
    }
    if (!*pkg) {
        *pkg = calloc(1, sizeof(**pkg));
        if (!*pkg) {
            return -1;
        }

        (*pkg)->metadata = calloc(1, sizeof(*(*pkg)->metadata));
        if (!(*pkg)->metadata) {
            guard_free(*pkg);
            return -1;
        }
    }
    if (wheel_get(pkg, filename) < 0) {
        return -1;
    }
    if (wheel_metadata_get(*pkg, filename) < 0) {
        return -1;
    }
    if (wheel_get_top_level(*pkg, filename) < 0) {
        return -1;
    }
    if (wheel_get_records(*pkg, filename) < 0) {
        return -1;
    }
    if (wheel_get_entry_point(*pkg, filename) < 0) {
        return -1;
    }

    // Optional marker
    wheel_get_zip_safe(*pkg, filename);

    return 0;
}


