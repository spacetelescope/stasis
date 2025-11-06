#include "envctl.h"

struct EnvCtl *envctl_init() {
    struct EnvCtl *result = calloc(1, sizeof(*result));
    if (!result) {
        return NULL;
    }

    result->num_alloc = STASIS_ENVCTL_DEFAULT_ALLOC;
    result->item = calloc(result->num_alloc + 1, sizeof(result->item));
    if (!result->item) {
        guard_free(result);
        return NULL;
    }

    return result;
}

static int callback_builtin_nop(const void *a, const void *b) {
    (void) a;  // Unused
    (void) b;  // Unused
    return STASIS_ENVCTL_RET_SUCCESS;
}

int envctl_register(struct EnvCtl **envctl, unsigned flags, envctl_except_fn *callback, const char *name) {
    if ((*envctl)->num_used == (*envctl)->num_alloc) {
        (*envctl)->num_alloc += STASIS_ENVCTL_DEFAULT_ALLOC;
        struct EnvCtl_Item **tmp = realloc((*envctl)->item, (*envctl)->num_alloc + 1 * sizeof((*envctl)->item));
        if (!tmp) {
            return 1;
        } else {
            (*envctl)->item = tmp;
        }
    }

    struct EnvCtl_Item **item = (*envctl)->item;
    item[(*envctl)->num_used] = calloc(1, sizeof(*item[0]));
    if (!item[(*envctl)->num_used]) {
        return 1;
    }
    if (!callback) {
        callback = &callback_builtin_nop;
    }
    item[(*envctl)->num_used]->callback = callback;
    item[(*envctl)->num_used]->name = name;
    item[(*envctl)->num_used]->flags = flags;

    (*envctl)->num_used++;
    return 0;
}

size_t envctl_get_index(const struct EnvCtl *envctl, const char *name) {
    for (size_t i = 0; i < envctl->num_used; i++) {
        if (!strcmp(envctl->item[i]->name, name)) {
            // pack state flag, outer (struct) index and inner (name) index
            return 1L << 63L | i;
        }
    }
    return 0;
}

void envctl_decode_index(size_t in_i, size_t *state, size_t *out_i, size_t *name_i) {
    (void) name_i;
    *state = ((in_i >> 63L) & 1);
    *out_i = in_i & 0xffffffffL;
}

unsigned envctl_check_required(unsigned flags) {
    return flags & STASIS_ENVCTL_REQUIRED;
}

unsigned envctl_check_redact(unsigned flags) {
    return flags & STASIS_ENVCTL_REDACT;
}

int envctl_check_present(const struct EnvCtl_Item *item, const char *name) {
    return ((!strcmp(item->name, name)) && getenv(name)) ? 1 : 0;
}

unsigned envctl_get_flags(const struct EnvCtl *envctl, const char *name) {
    size_t poll_index = envctl_get_index(envctl, name);
    size_t id = 0;
    size_t name_id = 0;
    size_t state = 0;
    envctl_decode_index(poll_index, &state, &id, &name_id);
    if (!state) {
        return 0;
    } else {
        fprintf(stderr, "managed environment variable: %s\n", name);
    }
    return envctl->item[id]->flags;
}

void envctl_do_required(const struct EnvCtl *envctl, int verbose) {
    for (size_t i = 0; i < envctl->num_used; i++) {
        struct EnvCtl_Item *item = envctl->item[i];
        const char *name = item->name;
        envctl_except_fn *callback = item->callback;

        if (verbose) {
            msg(STASIS_MSG_L2, "Verifying %s\n", name);
        }
        int code = callback((const void *) item, (const void *) name);
        if (code == STASIS_ENVCTL_RET_IGNORE || code == STASIS_ENVCTL_RET_SUCCESS) {
            continue;
        }
        if (code == STASIS_ENVCTL_RET_FAIL) {
            fprintf(stderr, "\n%s must be set. Exiting.\n", name);
            exit(1);
        }
        fprintf(stderr, "\nan unknown envctl callback code occurred: %d\n", code);
        exit(1);
    }
}

void envctl_free(struct EnvCtl **envctl) {
    if (!envctl) {
        return;
    }
    for (size_t i = 0; i < (*envctl)->num_used; i++) {
        guard_free((*envctl)->item[i]);
    }
    guard_free((*envctl)->item);
    guard_free(*envctl);
}