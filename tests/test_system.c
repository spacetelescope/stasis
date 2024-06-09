#include "testing.h"

static int ascii_file_contains(const char *filename, const char *value) {
    int result = -1;
    char *contents = omc_testing_read_ascii(filename);
    if (!contents) {
        perror(filename);
        return result;
    }
    result = strcmp(contents, value) == 0;
    guard_free(contents);
    return result;
}

void test_shell_output_null_args() {
    char *result;
    int status;
    result = shell_output(NULL, &status);
    OMC_ASSERT(strcmp(result, "") == 0, "no output expected");
    OMC_ASSERT(status != 0, "expected a non-zero exit code due to null argument string");
}

void test_shell_output_non_zero_exit() {
    char *result;
    int status;
    result = shell_output("HELLO1234 WORLD", &status);
    OMC_ASSERT(strcmp(result, "") == 0, "no output expected");
    OMC_ASSERT(status != 0, "expected a non-zero exit code due to intentional error");
}

void test_shell_output() {
    char *result;
    int status;
    result = shell_output("echo HELLO WORLD", &status);
    OMC_ASSERT(strcmp(result, "HELLO WORLD\n") == 0, "output message was incorrect");
    OMC_ASSERT(status == 0, "expected zero exit code");
}

void test_shell_safe() {
    struct Process proc;
    memset(&proc, 0, sizeof(proc));
    shell_safe(&proc, "true");
    OMC_ASSERT(proc.returncode == 0, "expected a zero exit code");
}

void test_shell_safe_verify_restrictions() {
    struct Process proc;

    const char *invalid_chars = OMC_SHELL_SAFE_RESTRICT;
    for (size_t i = 0; i < strlen(invalid_chars); i++) {
        char cmd[PATH_MAX] = {0};
        memset(&proc, 0, sizeof(proc));

        sprintf(cmd, "true%c false", invalid_chars[i]);
        shell_safe(&proc, cmd);
        OMC_ASSERT(proc.returncode == -1, "expected a negative result due to intentional error");
    }
}

void test_shell_null_proc() {
    int returncode = shell(NULL, "true");
    OMC_ASSERT(returncode == 0, "expected a zero exit code");
}

void test_shell_null_args() {
    struct Process proc;
    memset(&proc, 0, sizeof(proc));
    shell(&proc, NULL);
    OMC_ASSERT(proc.returncode < 0, "expected a non-zero/negative exit code");
}

void test_shell_exit() {
    struct Process proc;
    memset(&proc, 0, sizeof(proc));
    shell(&proc, "true");
    OMC_ASSERT(proc.returncode == 0, "expected a zero exit code");
}

void test_shell_non_zero_exit() {
    struct Process proc;
    memset(&proc, 0, sizeof(proc));
    shell(&proc, "false");
    OMC_ASSERT(proc.returncode != 0, "expected a non-zero exit code");
}

void test_shell() {
    struct Process procs[] = {
            {.f_stdout = "", .f_stderr = "", .redirect_stderr = 0, .returncode = 0},
            {.f_stdout = "stdout.log", .f_stderr = "", .redirect_stderr = 0, .returncode = 0},
            {.f_stdout = "", .f_stderr = "stderr.log", .redirect_stderr = 0, .returncode = 0},
            {.f_stdout = "stdouterr.log", .f_stderr = "", .redirect_stderr = 1, .returncode = 0},
    };
    for (size_t i = 0; i < sizeof(procs) / sizeof(*procs); i++) {
        shell(&procs[i], "echo test_stdout; echo test_stderr >&2");
        struct Process *proc = &procs[i];
        switch (i) {
            case 0:
                OMC_ASSERT(proc->returncode == 0, "echo should not fail");
                break;
            case 1:
                OMC_ASSERT(proc->returncode == 0, "echo should not fail");
                OMC_ASSERT(access(proc->f_stdout, F_OK) == 0, "stdout.log should exist");
                OMC_ASSERT(access(proc->f_stderr, F_OK) != 0, "stderr.log should not exist");
                OMC_ASSERT(ascii_file_contains(proc->f_stdout, "test_stdout\n"), "output file did not contain test message");
                remove(proc->f_stdout);
                break;
            case 2:
                OMC_ASSERT(proc->returncode == 0, "echo should not fail");
                OMC_ASSERT(access(proc->f_stdout, F_OK) != 0, "stdout.log should not exist");
                OMC_ASSERT(access(proc->f_stderr, F_OK) == 0, "stderr.log should exist");
                OMC_ASSERT(ascii_file_contains(proc->f_stderr, "test_stderr\n"), "output file did not contain test message");
                remove(proc->f_stderr);
                break;
            case 3:
                OMC_ASSERT(proc->returncode == 0, "echo should not fail");
                OMC_ASSERT(access("stdouterr.log", F_OK) == 0, "stdouterr.log should exist");
                OMC_ASSERT(access("stderr.log", F_OK) != 0, "stdout.log should not exist");
                OMC_ASSERT(access("stderr.log", F_OK) != 0, "stderr.log should not exist");
                OMC_ASSERT(ascii_file_contains(proc->f_stdout, "test_stdout\ntest_stderr\n"), "output file did not contain test message");
                remove(proc->f_stdout);
                remove(proc->f_stderr);
                break;
            default:
                break;
        }
    }
}

int main(int argc, char *argv[]) {
    OMC_TEST_BEGIN_MAIN();
    OMC_TEST_FUNC *tests[] = {
        test_shell_output_null_args,
        test_shell_output_non_zero_exit,
        test_shell_output,
        test_shell_safe_verify_restrictions,
        test_shell_safe,
        test_shell_null_proc,
        test_shell_null_args,
        test_shell_non_zero_exit,
        test_shell_exit,
        test_shell,
    };
    OMC_TEST_RUN(tests);
    OMC_TEST_END_MAIN();
}
