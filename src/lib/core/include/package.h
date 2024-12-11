#ifndef STASIS_PACKAGE_H
#define STASIS_PACKAGE_H

struct Package {
    struct {
        const char *name;
        const char *version_spec;
        const char *version;
    } meta;
    struct {
        const char *uri;
        unsigned handler;
    } source;
    struct {
        struct Test *test;
        size_t pass;
        size_t fail;
        size_t skip;
    };
    unsigned state;
};

struct Package *stasis_package_init(void);
void stasis_package_set_name(struct Package *pkg, const char *name);
void stasis_package_set_version(struct Package *pkg, const char *version);
void stasis_package_set_version_spec(struct Package *pkg, const char *version_spec);
void stasis_package_set_uri(struct Package *pkg, const char *uri);
void stasis_package_set_handler(struct Package *pkg, unsigned handler);

#endif //STASIS_PACKAGE_H
