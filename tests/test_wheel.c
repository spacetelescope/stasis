#include "testing.h"

void test_get_wheel_file() {
    struct testcase {
        const char *filename;
        struct Wheel expected;
    };
    struct testcase tc[] = {
        {
            // Test for "build tags"
            .filename = "btpackage-1.2.3-mytag-py2.py3-none-any.whl",
            .expected = {
                .file_name = "btpackage-1.2.3-mytag-py2.py3-none-any.whl",
                .version = "1.2.3",
                .distribution = "btpackage",
                .build_tag = "mytag",
                .platform_tag = "any",
                .python_tag = "py2.py3",
                .abi_tag = "none",
                .path_name = ".",
            }
        },
        {
            // Test for universal package format
            .filename = "anypackage-1.2.3-py2.py3-none-any.whl",
            .expected = {
                .file_name = "anypackage-1.2.3-py2.py3-none-any.whl",
                .version = "1.2.3",
                .distribution = "anypackage",
                .build_tag = NULL,
                .platform_tag = "any",
                .python_tag = "py2.py3",
                .abi_tag = "none",
                .path_name = ".",
            }
        },
        {
            // Test for binary package format
            .filename = "binpackage-1.2.3-cp311-cp311-linux_x86_64.whl",
            .expected = {
                .file_name = "binpackage-1.2.3-cp311-cp311-linux_x86_64.whl",
                .version = "1.2.3",
                .distribution = "binpackage",
                .build_tag = NULL,
                .platform_tag = "linux_x86_64",
                .python_tag = "cp311",
                .abi_tag = "cp311",
                .path_name = ".",
            }
        },
    };

    struct Wheel *doesnotexist = get_wheel_file("doesnotexist", "doesnotexist-0.0.1-py2.py3-none-any.whl", (char *[]) {"not", NULL}, WHEEL_MATCH_ANY);
    STASIS_ASSERT(doesnotexist == NULL, "returned non-NULL on error");

    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        struct testcase *test = &tc[i];
        struct Wheel *wheel = get_wheel_file(".", test->expected.distribution, (char *[]) {(char *) test->expected.version, NULL}, WHEEL_MATCH_ANY);
        STASIS_ASSERT(wheel != NULL, "result should not be NULL!");
        STASIS_ASSERT(wheel->file_name && strcmp(wheel->file_name, test->expected.file_name) == 0, "mismatched file name");
        STASIS_ASSERT(wheel->version && strcmp(wheel->version, test->expected.version) == 0, "mismatched version");
        STASIS_ASSERT(wheel->distribution && strcmp(wheel->distribution, test->expected.distribution) == 0, "mismatched distribution (package name)");
        STASIS_ASSERT(wheel->platform_tag && strcmp(wheel->platform_tag, test->expected.platform_tag) == 0, "mismatched platform tag ([platform]_[architecture])");
        STASIS_ASSERT(wheel->python_tag && strcmp(wheel->python_tag, test->expected.python_tag) == 0, "mismatched python tag (python version)");
        STASIS_ASSERT(wheel->abi_tag && strcmp(wheel->abi_tag, test->expected.abi_tag) == 0, "mismatched abi tag (python compatibility version)");
        if (wheel->build_tag) {
            STASIS_ASSERT(strcmp(wheel->build_tag, test->expected.build_tag) == 0,
                          "mismatched build tag (optional arbitrary string)");
        }
    }
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_get_wheel_file,
    };

    // Create mock package directories, and files
    mkdir("binpackage", 0755);
    touch("binpackage/binpackage-1.2.3-cp311-cp311-linux_x86_64.whl");
    mkdir("anypackage", 0755);
    touch("anypackage/anypackage-1.2.3-py2.py3-none-any.whl");
    mkdir("btpackage", 0755);
    touch("btpackage/btpackage-1.2.3-mytag-py2.py3-none-any.whl");

    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}