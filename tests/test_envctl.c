#include "testing.h"
#include "envctl.h"

void test_envctl_init() {
    struct EnvCtl *envctl;
    STASIS_ASSERT_FATAL((envctl = envctl_init()) != NULL, "envctl could not be initialized");
    STASIS_ASSERT(envctl->num_alloc == STASIS_ENVCTL_DEFAULT_ALLOC, "freshly initialized envctl does not have the correct number records");
    STASIS_ASSERT(envctl->num_used == 0, "freshly initialized envctl should have no allocations in use");
    STASIS_ASSERT(envctl->item != NULL, "freshly initialized envctl should have an empty items array. this one is NULL.");
    STASIS_ASSERT(envctl->item[0] == NULL, "freshly initialized envctl should not have any items. this one does.");
    envctl_free(&envctl);
    STASIS_ASSERT(envctl == NULL, "envctl should be NULL after envctl_free()");
}

static int except_passthru(const void *a, const void *b) {
    const struct EnvCtl_Item *item = a;
    const char *name = b;
    if (!envctl_check_required(item->flags) && envctl_check_present(item, name)) {
        return STASIS_ENVCTL_RET_SUCCESS;
    }
    return STASIS_ENVCTL_RET_FAIL;
}

static int except_required(const void *a, const void *b) {
    const struct EnvCtl_Item *item = a;
    const char *name = b;
    if (envctl_check_required(item->flags) && envctl_check_present(item, name)) {
        return STASIS_ENVCTL_RET_SUCCESS;
    }
    return STASIS_ENVCTL_RET_FAIL;
}

static int except_redact(const void *a, const void *b) {
    const struct EnvCtl_Item *item = a;
    const char *name = b;
    if (envctl_check_redact(item->flags) && envctl_check_present(item, name)) {
        return STASIS_ENVCTL_RET_SUCCESS;
    }
    return STASIS_ENVCTL_RET_FAIL;
}

void test_envctl_register() {
    struct EnvCtl *envctl;
    envctl = envctl_init();
    setenv("passthru", "true", 1);
    setenv("required", "true", 1);
    setenv("redact", "true", 1);
    envctl_register(&envctl, STASIS_ENVCTL_PASSTHRU, except_passthru, "passthru");
    envctl_register(&envctl, STASIS_ENVCTL_REQUIRED, except_required, "required");
    envctl_register(&envctl, STASIS_ENVCTL_REDACT, except_redact, "redact");

    unsigned flags[] = {
        STASIS_ENVCTL_PASSTHRU,
        STASIS_ENVCTL_REQUIRED,
        STASIS_ENVCTL_REDACT,
    };
    for (size_t i = 0; i < envctl->num_used; i++) {
        struct EnvCtl_Item *item = envctl->item[i];
        STASIS_ASSERT(item->flags == flags[i], "incorrect flag for item");
    }
    envctl_free(&envctl);
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_envctl_init,
        test_envctl_register,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}