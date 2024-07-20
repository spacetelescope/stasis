#ifndef STASIS_ENVCTL_H
#define STASIS_ENVCTL_H

#include <stdlib.h>

#define STASIS_ENVCTL_PASSTHRU 0
#define STASIS_ENVCTL_REQUIRED 1 << 1
#define STASIS_ENVCTL_REDACT 1 << 2
#define STASIS_ENVCTL_DEFAULT_ALLOC 100

#define STASIS_ENVCTL_RET_FAIL (-1)
#define STASIS_ENVCTL_RET_SUCCESS 1
#define STASIS_ENVCTL_RET_IGNORE 2
typedef int (envctl_except_fn)(const void *, const void *);

struct EnvCtl_Item {
    unsigned flags; //<! One or more STASIS_ENVCTL_* flags
    const char *name; //<! Environment variable name
    envctl_except_fn *callback;
};

struct EnvCtl {
    size_t num_alloc;
    size_t num_used;
    struct EnvCtl_Item **item;
};

struct EnvCtl *envctl_init();
int envctl_register(struct EnvCtl **envctl, unsigned flags, envctl_except_fn *callback, const char *name);
unsigned envctl_get_flags(const struct EnvCtl *envctl, const char *name);
unsigned envctl_check_required(unsigned flags);
unsigned envctl_check_redact(unsigned flags);
int envctl_check_present(const struct EnvCtl_Item *item, const char *name);
void envctl_do_required(const struct EnvCtl *envctl, int verbose);
void envctl_free(struct EnvCtl **envctl);

#endif // STASIS_ENVCTL_H