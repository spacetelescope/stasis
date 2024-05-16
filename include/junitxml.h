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
    char *name;
    int errors;
    int failures;
    int skipped;
    int tests;
    float time;
    char *timestamp;
    char *hostname;
    struct JUNIT_Testcase **testcase;
    size_t _tc_inuse;
    size_t _tc_alloc;
};

struct JUNIT_Testsuite *junitxml_testsuite_read(const char *filename);
void junitxml_testsuite_free(struct JUNIT_Testsuite **testsuite);

#endif //OMC_JUNITXML_H
