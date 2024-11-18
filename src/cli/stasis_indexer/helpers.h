#ifndef HELPERS_H
#define HELPERS_H

#include "delivery.h"

struct StrList *get_architectures(struct Delivery ctx[], size_t nelem);
struct StrList *get_platforms(struct Delivery ctx[], size_t nelem);
int get_pandoc_version(size_t *result);
int get_latest_rc(struct Delivery ctx[], size_t nelem);
struct Delivery **get_latest_deliveries(struct Delivery ctx[], size_t nelem);
int get_files(struct StrList **out, const char *path, const char *pattern, ...);
int load_metadata(struct Delivery *ctx, const char *filename);
int micromamba_configure(const struct Delivery *ctx, struct MicromambaInfo *m);

#endif //HELPERS_H
