#ifndef WHEEL_H
#define WHEEL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include <zip.h>

#define WHEEL_MAXELEM 255

#define WHEEL_FROM_DIST 0
#define WHEEL_FROM_METADATA 1

enum {
    WHEEL_META_METADATA_VERSION=0,
    WHEEL_META_NAME,
    WHEEL_META_VERSION,
    WHEEL_META_AUTHOR,
    WHEEL_META_AUTHOR_EMAIL,
    WHEEL_META_MAINTAINER,
    WHEEL_META_MAINTAINER_EMAIL,
    WHEEL_META_SUMMARY,
    WHEEL_META_LICENSE,
    WHEEL_META_LICENSE_EXPRESSION,
    WHEEL_META_LICENSE_FILE,
    WHEEL_META_HOME_PAGE,
    WHEEL_META_DOWNLOAD_URL,
    WHEEL_META_PROJECT_URL,
    WHEEL_META_CLASSIFIER,
    WHEEL_META_REQUIRES_PYTHON,
    WHEEL_META_REQUIRES_EXTERNAL,
    WHEEL_META_IMPORT_NAME,
    WHEEL_META_IMPORT_NAMESPACE,
    WHEEL_META_REQUIRES_DIST,
    WHEEL_META_PROVIDES,
    WHEEL_META_PROVIDES_DIST,
    WHEEL_META_PROVIDES_EXTRA,
    WHEEL_META_OBSOLETES,
    WHEEL_META_OBSOLETES_DIST,
    WHEEL_META_PLATFORM,
    WHEEL_META_SUPPORTED_PLATFORM,
    WHEEL_META_KEYWORDS,
    WHEEL_META_DYNAMIC,
    WHEEL_META_DESCRIPTION_CONTENT_TYPE,
    WHEEL_META_DESCRIPTION,
    WHEEL_META_END_ENUM, // NOP
};


enum {
    WHEEL_DIST_VERSION=0,
    WHEEL_DIST_GENERATOR,
    WHEEL_DIST_ROOT_IS_PURELIB,
    WHEEL_DIST_TAG,
    WHEEL_DIST_ZIP_SAFE,
    WHEEL_DIST_TOP_LEVEL,
    WHEEL_DIST_ENTRY_POINT,
    WHEEL_DIST_RECORD,
    WHEEL_DIST_END_ENUM, // NOP
};

struct WheelMetadata_ProvidesExtra {
    char *target;
    struct StrList *requires_dist;
    int count;
};

struct WheelMetadata {
    char *metadata_version;
    char *name;
    char *version;
    char *summary;
    struct StrList *author;
    struct StrList *author_email;
    struct StrList *maintainer;
    struct StrList *maintainer_email;
    char *license;
    char *license_expression;
    char *home_page;
    char * download_url;
    struct StrList *project_url;
    struct StrList *classifier;
    struct StrList *requires_python;
    struct StrList *requires_external;
    char *description_content_type;
    struct StrList *license_file;
    struct StrList *import_name;
    struct StrList *import_namespace;
    struct StrList *requires_dist;
    struct StrList *provides;
    struct StrList *provides_dist;
    struct StrList *obsoletes;
    struct StrList *obsoletes_dist;
    char *description;
    struct StrList *platform;
    struct StrList *supported_platform;
    struct StrList *keywords;
    struct StrList *dynamic;

    struct WheelMetadata_ProvidesExtra **provides_extra;
};

struct WheelRecord {
    char *filename;
    char *checksum;
    size_t size;
};

struct WheelEntryPoint {
    char *name;
    char *function;
    char *type;
};

/*
Wheel-Version: 1.0
Generator: setuptools (75.8.0)
Root-Is-Purelib: false
Tag: cp313-cp313-manylinux_2_17_x86_64
Tag: cp313-cp313-manylinux2014_x86_64
*/

struct Wheel {
    char *wheel_version;
    char *generator;
    char *root_is_pure_lib;
    struct StrList *tag;
    struct StrList *top_level;
    int zip_safe;
    struct WheelMetadata *metadata;
    struct WheelRecord **record;
    size_t num_record;
    struct WheelEntryPoint **entry_point;
    size_t num_entry_point;
};

#define METADATA_MULTILINE_PREFIX "        "


static inline int consume_append(char **dest, const char *src, const char *accept) {
    const char *start = src;
    if (!strncmp(src, METADATA_MULTILINE_PREFIX, strlen(METADATA_MULTILINE_PREFIX))) {
        start += strlen(METADATA_MULTILINE_PREFIX);
    }

    const char *end = strpbrk(start, accept);
    size_t cut_len = end ? (size_t)(end - start) : strlen(start);
    size_t dest_len = strlen(*dest);

    char *tmp = realloc(*dest, strlen(*dest) + cut_len + 2);
    if (!tmp) {
        return -1;
    }
    *dest = tmp;
    memcpy(*dest + dest_len, start, cut_len);

    dest_len += cut_len;
    (*dest)[dest_len ? dest_len : 0] = '\n';
    (*dest)[dest_len + 1] = '\0';
    return 0;
}

#define WHEEL_KEY_UNKNOWN (-1)
enum {
    WHEELVAL_STR = 0,
    WHEELVAL_STRLIST,
    WHEELVAL_OBJ_EXTRA,
    WHEELVAL_OBJ_RECORD,
    WHEELVAL_OBJ_ENTRY_POINT,
};

struct WheelValue {
    int type;
    size_t count;
    void *data;
};

int wheel_package(struct Wheel **pkg, const char *filename);
void wheel_package_free(struct Wheel **pkg);
struct WheelValue wheel_get_value_by_name(const struct Wheel *pkg, int from, const char *key);
struct WheelValue wheel_get_value_by_id(const struct Wheel *pkg, int from, ssize_t id);
int wheel_value_error(struct WheelValue const *val);
const char *wheel_get_key_by_id(int from, ssize_t id);
int wheel_get_file_contents(const char *wheelfile, const char *filename, char **contents);
int wheel_show_info(const struct Wheel *wheel);

#endif //WHEEL_H
