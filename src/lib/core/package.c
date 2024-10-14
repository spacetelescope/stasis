#include <stdlib.h>
#include "package.h"
#include "core.h"

struct Package *stasis_package_init() {
    struct Package *result;
    result = calloc(1, sizeof(*result));
    return result;
}

void stasis_package_set_name(struct Package *pkg, const char *name) {
    if (pkg->meta.name) {
        guard_free(pkg->meta.name);
    }
    pkg->meta.name = strdup(name);
}

void stasis_package_set_version(struct Package *pkg, const char *version) {
    if (pkg->meta.version) {
        guard_free(pkg->meta.version);
    }
    pkg->meta.version = strdup(version);
}

void stasis_package_set_version_spec(struct Package *pkg, const char *version_spec) {
    if (pkg->meta.version_spec) {
        guard_free(pkg->meta.version_spec);
    }
    pkg->meta.version_spec = strdup(version_spec);
}

void stasis_package_set_uri(struct Package *pkg, const char *uri) {
    if (pkg->source.uri) {
        guard_free(pkg->source.uri);
    }
    pkg->source.uri = uri;
}

void stasis_package_set_handler(struct Package *pkg, unsigned handler) {
    pkg->source.handler = handler;
}