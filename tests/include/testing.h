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

extern inline void stasis_testing_setup_workspace();
extern inline void stasis_testing_clean_up_docker();
extern inline void stasis_testing_teardown_workspace();
extern inline void stasis_testing_record_result(struct stasis_test_result_t result);
extern inline int stasis_testing_has_failed();
extern inline void stasis_testing_record_result_summary();
extern inline char *stasis_testing_read_ascii(const char *filename);
extern inline int stasis_testing_write_ascii(const char *filename, const char *data);

inline void stasis_testing_record_result(struct stasis_test_result_t result) {
    memcpy(&stasis_test_results[stasis_test_results_i], &result, sizeof(result));
    stasis_test_results_i++;
}

inline int stasis_testing_has_failed() {
    for (size_t i = 0; i < stasis_test_results_i; i++) {
        if (stasis_test_results[i].status == false) {
            return 1;
        }
    }
    return 0;
}

inline void stasis_testing_record_result_summary() {
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

inline char *stasis_testing_read_ascii(const char *filename) {
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

    char *result = NULL;
    result = calloc(st.st_size + 1, sizeof(*result));
    if (fread(result, sizeof(*result), st.st_size, fp) < 1) {
        guard_free(result);
        perror(filename);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return result;
}

inline int stasis_testing_write_ascii(const char *filename, const char *data) {
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

char TEST_DATA_DIR[PATH_MAX] = {0};
char TEST_START_DIR[PATH_MAX] = {0};
char TEST_WORKSPACE_DIR[PATH_MAX] = {0};
inline void stasis_testing_setup_workspace() {
    if (!realpath("data", TEST_DATA_DIR)) {
        SYSERROR("%s", "Data directory is missing");
        exit(1);
    }

    if (mkdir("workspaces", 0755) < 0) {
        if (errno != EEXIST) {
            SYSERROR("%s", "Unable to create workspaces directory");
            exit(1);
        }
    }
    char ws[] = "workspaces/workspace_XXXXXX";
    if (mkdtemp(ws) == NULL) {
        SYSERROR("Unable to create testing workspace: %s", ws);
        exit(1);
    }
    if (!realpath(ws, TEST_WORKSPACE_DIR)) {
        SYSERROR("%s", "Unable to determine absolute path to temporary workspace");
        exit(1);
    }
    if (chdir(TEST_WORKSPACE_DIR) < 0) {
        SYSERROR("Unable to enter workspace directory: '%s'", TEST_WORKSPACE_DIR);
        exit(1);
    }
    if (setenv("HOME", TEST_WORKSPACE_DIR, 1) < 0) {
        SYSERROR("Unable to reset HOME to '%s'", TEST_WORKSPACE_DIR);
    }
}

inline void stasis_testing_clean_up_docker() {
    char containers_dir[PATH_MAX] = {0};
    snprintf(containers_dir, sizeof(containers_dir) - 1, "%s/.local/share/containers", TEST_WORKSPACE_DIR);

    if (access(containers_dir, F_OK) == 0) {
        char cmd[PATH_MAX] = {0};
        snprintf(cmd, sizeof(cmd) - 1, "docker run --rm -it -v %s:/data alpine sh -c '/bin/rm -r -f /data/*' &>/dev/null", containers_dir);
        // This command will "fail" due to podman's internal protection(s). However, this gets us close enough.
        system(cmd);

        // Podman seems to defer the rollback operation on-error for a short period.
        // This buys time, so we can delete it.
        sleep(1);
        sync();
        if (rmtree(containers_dir)) {
            SYSERROR("WARNING: Unable to fully remove container directory: '%s'", containers_dir);
        }
    }
}

inline void stasis_testing_teardown_workspace() {
    if (chdir(TEST_START_DIR) < 0) {
        SYSERROR("Unable to re-enter test start directory from workspace directory: %s", TEST_START_DIR);
        exit(1);
    }
    if (!getenv("KEEP_WORKSPACE")) {
        if (strlen(TEST_WORKSPACE_DIR) > 1) {
            stasis_testing_clean_up_docker();
            rmtree(TEST_WORKSPACE_DIR);
        }
    }
}

#define STASIS_TEST_BEGIN_MAIN() do { \
        setenv("PYTHONUNBUFFERED", "1", 1); \
        fflush(stdout); \
        fflush(stderr); \
        setvbuf(stdout, NULL, _IONBF, 0); \
        setvbuf(stderr, NULL, _IONBF, 0); \
        if (!getcwd(TEST_START_DIR, sizeof(TEST_START_DIR) - 1)) { \
            SYSERROR("%s", "Unable to determine current working directory"); \
            exit(1); \
        } \
        atexit(stasis_testing_record_result_summary); \
        atexit(stasis_testing_teardown_workspace); \
        stasis_testing_setup_workspace(); \
    } while (0)
#define STASIS_TEST_END_MAIN() do { return stasis_testing_has_failed(); } while (0)

#define STASIS_ASSERT(COND, REASON) do { \
        stasis_testing_record_result((struct stasis_test_result_t) { \
            .filename = __FILE_NAME__, \
            .funcname = __FUNCTION__,  \
            .lineno = __LINE__,        \
            .status = (COND),             \
            .msg_assertion = "ASSERT(" #COND ")",                 \
            .msg_reason = (REASON) } );  \
    } while (0)

#define STASIS_ASSERT_FATAL(COND, REASON) do { \
        stasis_testing_record_result((struct stasis_test_result_t) { \
            .filename = __FILE_NAME__, \
            .funcname = __FUNCTION__,  \
            .lineno = __LINE__,        \
            .status = (COND),             \
            .msg_assertion = "ASSERT FATAL (" #COND ")",                 \
            .msg_reason = (REASON) }       \
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
            .msg_reason = (REASON) }       \
        );    \
        if (stasis_test_results[stasis_test_results_i ? stasis_test_results_i - 1 : stasis_test_results_i].skip == true) {\
            return; \
        } \
    } while (0)

#define STASIS_TEST_RUN(X) do { \
        for (size_t i = 0; i < sizeof(X) / sizeof(*(X)); i++) { \
            if ((X)[i]) { \
                (X)[i](); \
            } \
        } \
    } while (0)

#endif //STASIS_TESTING_H
