#ifndef STASIS_TESTING_H
#define STASIS_TESTING_H
#include "core.h"
#define STASIS_TEST_RUN_MAX 10000
#define STASIS_TEST_SUITE_FATAL 1
#define STASIS_TEST_SUITE_SKIP 127

#ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif

typedef void(STASIS_TEST_FUNC)();
struct stasis_test_result_t {
    const char *filename;
    const char *funcname;
    int lineno;
    const char *msg_assertion;
    const char *msg_reason;
    const int status;
    const int skip;
} stasis_test_results[STASIS_TEST_RUN_MAX];
size_t stasis_test_results_i = 0;

void stasis_testing_record_result(struct stasis_test_result_t result);

void stasis_testing_record_result(struct stasis_test_result_t result) {
    memcpy(&stasis_test_results[stasis_test_results_i], &result, sizeof(result));
    stasis_test_results_i++;
}

int stasis_testing_has_failed() {
    for (size_t i = 0; i < stasis_test_results_i; i++) {
        if (stasis_test_results[i].status == false) {
            return 1;
        }
    }
    return 0;
}
void stasis_testing_record_result_summary() {
    size_t failed = 0;
    size_t skipped = 0;
    size_t passed = 0;
    int do_message;
    static char status_msg[255] = {0};
    for (size_t i = 0; i < stasis_test_results_i; i++) {
        if (stasis_test_results[i].status && stasis_test_results[i].skip) {
            strcpy(status_msg, "SKIP");
            do_message = 1;
            skipped++;
        } else if (!stasis_test_results[i].status) {
            strcpy(status_msg, "FAIL");
            do_message = 1;
            failed++;
        } else {
            strcpy(status_msg, "PASS");
            do_message = 0;
            passed++;
        }
        fprintf(stdout, "[%s] %s:%d :: %s() => %s",
               status_msg,
               stasis_test_results[i].filename,
               stasis_test_results[i].lineno,
               stasis_test_results[i].funcname,
               stasis_test_results[i].msg_assertion);
        if (do_message) {
            fprintf(stdout, "\n      \\_ %s", stasis_test_results[i].msg_reason);
        }
        fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n[UNIT] %zu tests passed, %zu tests failed, %zu tests skipped out of %zu\n", passed, failed, skipped, stasis_test_results_i);
}

char *stasis_testing_read_ascii(const char *filename) {
    struct stat st;
    if (stat(filename, &st)) {
        perror(filename);
        return NULL;
    }

    FILE *fp;
    fp = fopen(filename, "r");
    if (!fp) {
        perror(filename);
        return NULL;
    }

    char *result;
    result = calloc(st.st_size + 1, sizeof(*result));
    if (fread(result, sizeof(*result), st.st_size, fp) < 1) {
        perror(filename);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return result;
}

int stasis_testing_write_ascii(const char *filename, const char *data) {
    FILE *fp;
    fp = fopen(filename, "w+");
    if (!fp) {
        perror(filename);
        return -1;
    }

    if (!fprintf(fp, "%s", data)) {
        perror(filename);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

#define STASIS_TEST_BEGIN_MAIN() do { \
        setenv("PYTHONUNBUFFERED", "1", 1); \
        fflush(stdout); \
        fflush(stderr); \
        setvbuf(stdout, NULL, _IONBF, 0); \
        setvbuf(stderr, NULL, _IONBF, 0); \
        atexit(stasis_testing_record_result_summary); \
    } while (0)
#define STASIS_TEST_END_MAIN() do { return stasis_testing_has_failed(); } while (0)

#define STASIS_ASSERT(COND, REASON) do { \
        stasis_testing_record_result((struct stasis_test_result_t) { \
            .filename = __FILE_NAME__, \
            .funcname = __FUNCTION__,  \
            .lineno = __LINE__,        \
            .status = (COND),             \
            .msg_assertion = "ASSERT(" #COND ")",                 \
            .msg_reason = REASON } );  \
    } while (0)

#define STASIS_ASSERT_FATAL(COND, REASON) do { \
        stasis_testing_record_result((struct stasis_test_result_t) { \
            .filename = __FILE_NAME__, \
            .funcname = __FUNCTION__,  \
            .lineno = __LINE__,        \
            .status = (COND),             \
            .msg_assertion = "ASSERT FATAL (" #COND ")",                 \
            .msg_reason = REASON }       \
        );    \
        if (stasis_test_results[stasis_test_results_i ? stasis_test_results_i - 1 : stasis_test_results_i].status == false) {\
            exit(STASIS_TEST_SUITE_FATAL); \
        } \
    } while (0)

#define STASIS_SKIP_IF(COND, REASON) do { \
        stasis_testing_record_result((struct stasis_test_result_t) { \
            .filename = __FILE_NAME__, \
            .funcname = __FUNCTION__,  \
            .lineno = __LINE__,        \
            .status = true, \
            .skip = (COND), \
            .msg_assertion = "SKIP (" #COND ")",                 \
            .msg_reason = REASON }       \
        );    \
        if (stasis_test_results[stasis_test_results_i ? stasis_test_results_i - 1 : stasis_test_results_i].skip == true) {\
            return; \
        } \
    } while (0)

#define STASIS_TEST_RUN(X) do { \
        for (size_t i = 0; i < sizeof(X) / sizeof(*X); i++) { \
            if (X[i]) { \
                X[i](); \
            } \
        } \
    } while (0)

#endif //STASIS_TESTING_H
