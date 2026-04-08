#include "delivery.h"
#include "testing.h"

static struct Test *mock_test(const int ident) {
    struct Test *test = test_init();
    if (asprintf(&test->name, "test_%d", ident) < 0) {
        return NULL;
    }
    return test;
}

void test_tests() {
    const int initial = TEST_NUM_ALLOC_INITIAL;
    const int balloon = initial * 10;
    struct Tests *tests = tests_init(initial);
    STASIS_ASSERT_FATAL(tests != NULL, "tests structure allocation failed");
    STASIS_ASSERT(tests->num_alloc == (size_t) initial, "incorrect number of records initialized");
    STASIS_ASSERT(tests->num_used == 0, "incorrect number of records used");

    for (int i = 0; i < balloon; i++) {
        struct Test *test = mock_test(i);
        if (!test) {
            SYSERROR("unable to allocate memory for test %d", i);
            return;
        }
        tests_add(tests, test);
    }

    size_t errors = 0;
    for (int i = 0; i < initial * 10; i++) {
        struct Test *test = tests->test[i];
        if (!test) {
            errors++;
            continue;
        }
        if (!test->name) {
            errors++;
        }
    }
    STASIS_ASSERT(errors == 0, "no errors should be detected in test->name member");

    tests_free(&tests);
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_tests,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}