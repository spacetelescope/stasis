#include "testing.h"
#include "junitxml.h"

void test_junitxml_testsuite_read() {
    struct JUNIT_Testsuite *testsuite;
    STASIS_ASSERT_FATAL((testsuite = junitxml_testsuite_read("result.xml")) != NULL, "failed to load testsuite data");
    STASIS_ASSERT(testsuite->name != NULL, "Test suite must be named");
    STASIS_ASSERT(testsuite->skipped > 0, "missed skipped tests");
    STASIS_ASSERT(testsuite->failures > 0, "missed failed tests");
    STASIS_ASSERT(testsuite->errors == 0, "should not have errored tests");
    STASIS_ASSERT(testsuite->tests > 0, "missed tests");

    for (size_t i = 0; i < testsuite->_tc_inuse; i++) {
        struct JUNIT_Testcase *testcase = testsuite->testcase[i];
        STASIS_ASSERT_FATAL(testcase->name != NULL, "test case should not be NULL");
        STASIS_ASSERT(testcase->name != NULL, "name should not be NULL");
        STASIS_ASSERT(testcase->classname != NULL, "classname should not be NULL");

        switch (testcase->tc_result_state_type) {
            case JUNIT_RESULT_STATE_SKIPPED:
                STASIS_ASSERT(testcase->result_state.skipped != NULL, "skipped state set, but data pointer was null");
                STASIS_ASSERT(testcase->result_state.skipped->message != NULL, "data pointer set, but message pointer was null");
                break;
            case JUNIT_RESULT_STATE_FAILURE:
                STASIS_ASSERT(testcase->result_state.failure != NULL, "failure state set, but data pointer was null");
                STASIS_ASSERT(testcase->result_state.failure->message != NULL, "data pointer set, but message pointer was null");
                break;
            case JUNIT_RESULT_STATE_ERROR:
                STASIS_ASSERT(testcase->result_state.error != NULL, "error state set, but data pointer was null");
                STASIS_ASSERT(testcase->result_state.error->message != NULL, "data pointer set, but message pointer was null");
                break;
            case JUNIT_RESULT_STATE_NONE:
                STASIS_ASSERT(testcase->result_state.failure == NULL, "success, but has an failure record");
                STASIS_ASSERT(testcase->result_state.skipped == NULL, "success, but has a skipped record ");
                STASIS_ASSERT(testcase->result_state.error == NULL, "success, but has an error record");
                break;
            default:
                SYSERROR("unknown test case result type (%d)", testcase->tc_result_state_type);
                break;
        }
    }
    junitxml_testsuite_free(&testsuite);
}

void test_junitxml_testsuite_read_error() {
    struct JUNIT_Testsuite *testsuite;
    STASIS_ASSERT_FATAL((testsuite = junitxml_testsuite_read("result_error.xml")) != NULL, "failed to load testsuite data");

    STASIS_ASSERT(testsuite->name != NULL, "test suite must be named");
    STASIS_ASSERT(testsuite->skipped == 0, "should not have any skipped tests");
    STASIS_ASSERT(testsuite->failures == 0, "should not have any failures, only errors");
    STASIS_ASSERT(testsuite->errors > 0, "missed failed tests");
    STASIS_ASSERT(testsuite->tests > 0, "missed tests");
    STASIS_ASSERT(testsuite->timestamp != NULL, "Test suite must have a timestamp");

    for (size_t i = 0; i < testsuite->_tc_inuse; i++) {
        struct JUNIT_Testcase *testcase = testsuite->testcase[i];
        STASIS_ASSERT_FATAL(testcase->name != NULL, "test case should not be NULL");
        STASIS_ASSERT(testcase->name != NULL, "name should not be NULL");
        STASIS_ASSERT(testcase->classname != NULL, "classname should not be NULL");

        switch (testcase->tc_result_state_type) {
            case JUNIT_RESULT_STATE_ERROR:
                STASIS_ASSERT(testcase->result_state.error != NULL, "error state set, but data pointer was null");
                STASIS_ASSERT(testcase->result_state.error->message != NULL, "data pointer set, but message pointer was null");
                break;
            default:
                SYSERROR("unexpected test case result type (%d)", testcase->tc_result_state_type);
                break;
        }
    }
    junitxml_testsuite_free(&testsuite);
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_junitxml_testsuite_read,
        test_junitxml_testsuite_read_error,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}