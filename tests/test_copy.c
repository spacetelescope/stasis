#include "testing.h"


void test_copy() {
    struct testcase {
        const char *in_file;
        char *out_file;
        int expect_size;
        int expect_return;
    };
    struct testcase tc[] = {
        {.in_file = "file_to_copy.txt", .out_file = "file_copied_1000.txt", .expect_size = 0x1000, .expect_return = 0},
        {.in_file = "file_to_copy.txt", .out_file = "file_copied_4000.txt", .expect_size = 0x4000, .expect_return = 0},
        {.in_file = "file_to_copy.txt", .out_file = "file_copied_8000.txt", .expect_size = 0x8000, .expect_return = 0},
        {.in_file = "file_to_copy.txt", .out_file = "file_copied_100000.txt", .expect_size = 0x100000, .expect_return = 0},
    };

    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        struct stat st_a, st_b;
        struct testcase *test = &tc[i];
        char *mock_data = malloc(test->expect_size * sizeof(*mock_data));
        memset(mock_data, 'A', test->expect_size);
        mock_data[test->expect_size] = '\0';
        stasis_testing_write_ascii(test->in_file, mock_data);
        guard_free(mock_data);

        STASIS_ASSERT(copy2(test->in_file, test->out_file, CT_OWNER | CT_PERM) == test->expect_return, "copy2 failed");
        STASIS_ASSERT(stat(test->in_file, &st_a) == 0, "source stat failed");
        STASIS_ASSERT(stat(test->in_file, &st_b) == 0, "destination stat failed");
        STASIS_ASSERT(st_b.st_size == test->expect_size, "destination file is not the expected size");
        STASIS_ASSERT(st_a.st_size == st_b.st_size, "source and destination files should be the same size");
        STASIS_ASSERT(st_a.st_mode == st_b.st_mode, "source and destination files should have the same permissions");
        STASIS_ASSERT(st_a.st_uid == st_b.st_uid, "source and destination should have the same UID");
    }
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_copy,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}