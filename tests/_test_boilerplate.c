#include "testing.h"

void test_NAME() {
    struct testcase {

    };
    STASIS_ASSERT();
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_NAME,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}