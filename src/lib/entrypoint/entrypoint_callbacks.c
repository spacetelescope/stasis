#include "entrypoint_callbacks.h"

int callback_except_jf(const void *a, const void *b) {
    const struct EnvCtl_Item *item = a;
    const char *name = b;

    if (!globals.enable_artifactory) {
        return STASIS_ENVCTL_RET_IGNORE;
    }

    if (envctl_check_required(item->flags)) {
        const char *content = getenv(name);
        if (!content || isempty((char *) content)) {
            return STASIS_ENVCTL_RET_FAIL;
        }
    }

    return STASIS_ENVCTL_RET_SUCCESS;
}

int callback_except_gh(const void *a, const void *b) {
    const struct EnvCtl_Item *item = a;
    const char *name = b;
    //printf("GH exception check: %s\n", name);
    if (envctl_check_required(item->flags) && envctl_check_present(item, name)) {
        return STASIS_ENVCTL_RET_SUCCESS;
    }

    return STASIS_ENVCTL_RET_FAIL;
}

