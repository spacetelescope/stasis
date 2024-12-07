#ifndef HELPERS_H
#define HELPERS_H

#include "delivery.h"

#define ARRAY_COUNT_DYNAMIC(X, COUNTER) \
    do { \
        for (COUNTER = 0; X && X[COUNTER] != NULL; COUNTER++) {} \
    } while(0)

#define ARRAY_COUNT_BY_STRUCT_MEMBER(X, MEMBER, COUNTER) \
    do { \
        for (COUNTER = 0; X[COUNTER].MEMBER != NULL; COUNTER++) {} \
    } while(0)

struct StrList *get_architectures(struct Delivery ctx[], size_t nelem);
struct StrList *get_platforms(struct Delivery ctx[], size_t nelem);
int get_pandoc_version(size_t *result);
int pandoc_exec(const char *in_file, const char *out_file, const char *css_file, const char *title);
int get_latest_rc(struct Delivery ctx[], size_t nelem);
struct Delivery *get_latest_deliveries(struct Delivery ctx[], size_t nelem);
int get_files(struct StrList **out, const char *path, const char *pattern, ...);
struct StrList *get_docker_images(struct Delivery *ctx, char *pattern);
int load_metadata(struct Delivery *ctx, const char *filename);
int micromamba_configure(const struct Delivery *ctx, struct MicromambaInfo *m);

#endif //HELPERS_H
