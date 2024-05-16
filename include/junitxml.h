// @file junitxml.h
#ifndef OMC_JUNITXML_H
#define OMC_JUNITXML_H
#include <libxml/xmlreader.h>

struct JUNIT_Failure {
    char *message;
};

struct JUNIT_Skipped {
    char *type;
    char *message;
};

#define JUNIT_RESULT_STATE_NONE 0
#define JUNIT_RESULT_STATE_FAILURE 1
#define JUNIT_RESULT_STATE_SKIPPED 2
struct JUNIT_Testcase {
    char *classname;
    char *name;
    float time;
    char *message;
    int tc_result_state_type;
    union tc_state_ptr {
        struct JUNIT_Failure *failure;
        struct JUNIT_Skipped *skipped;
    } result_state;
};

struct JUNIT_Testsuite {
    //< Test suite name
    char *name;
    //< Total number of test terminated due to an error
    int errors;
    //< Total number of failed tests
    int failures;
    //< Total number of skipped tests
    int skipped;
    //< Total number of tests
    int tests;
    //< Total duration in fractional seconds
    float time;
    //< Timestamp
    char *timestamp;
    //< Test runner host name
    char *hostname;
    //< Array of test cases
    struct JUNIT_Testcase **testcase;
    //< Total number of test cases in use
    size_t _tc_inuse;
    //< Total number of test cases allocated
    size_t _tc_alloc;
};

/**
 * Extract information from a junit XML file
 *
 * ~~~{.c}
 * struct JUNIT_Testsuite *testsuite;
 * const char *filename = "/path/to/result.xml";
 *
 * testsuite = junitxml_testsuite_read(filename);
 * if (testsuite) {
 *     // Did any test cases fail?
 *     if (testsuite->failures) {
 *         printf("Test suite '%s' has %d failure(s)\n", testsuite->name, testsuite->failures
 *         // Scan test cases for failure data
 *         for (size_t i = 0; i < testsuite->_tc_inuse; i++) {
 *             // Check result state (one of)
 *             //   JUNIT_RESULT_STATE_FAILURE
 *             //   JUNIT_RESULT_STATE_SKIPPED
 *             struct JUNIT_Testcase testcase = testsuite->testcase[i];
 *             if (testcase->tc_result_state_type) {
 *                 if (testcase->tc_result_state_type == JUNIT_RESULT_STATE_FAILURE) {
 *                     // Display information from failed test case
 *                     printf("[FAILED] %s::%s\nOutput:\n%s\n",
 *                         testcase->classname,
 *                         testcase->name,
 *                         testcase->result_state.failure->message);
 *                 }
 *             }
 *         }
 *     }
 *     // Release test suite resources
 *     junitxml_testsuite_free(&testsuite);
 * } else {
 *     // handle error
 * }
 *
 *
 * ~~~
 *
 * @param filename path to junit XML file
 * @return pointer to JUNIT_Testsuite
 */
struct JUNIT_Testsuite *junitxml_testsuite_read(const char *filename);

/**
 * Free memory allocated by junitxml_testsuite_read
 * @param testsuite pointer to JUNIT_Testsuite
 */
void junitxml_testsuite_free(struct JUNIT_Testsuite **testsuite);

#endif //OMC_JUNITXML_H
