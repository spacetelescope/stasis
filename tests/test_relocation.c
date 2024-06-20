#include "testing.h"

static const char *test_string = "The quick brown fox jumps over the lazy dog.";
static const char *targets[] = {
        "The", "^^^ quick brown fox jumps over the lazy dog.",
        "quick", "The ^^^ brown fox jumps over the lazy dog.",
        "brown fox jumps over the", "The quick ^^^ lazy dog.",
        "lazy", "The quick brown fox jumps over the ^^^ dog.",
        "dog", "The quick brown fox jumps over the lazy ^^^.",
};

void test_replace_text() {
    for (size_t i = 0; i < sizeof(targets) / sizeof(*targets); i += 2) {
        const char *target = targets[i];
        const char *expected = targets[i + 1];
        char input[BUFSIZ] = {0};
        strcpy(input, test_string);

        OMC_ASSERT(replace_text(input, target, "^^^", 0) == 0, "string replacement failed");
        OMC_ASSERT(strcmp(input, expected) == 0, "unexpected replacement");
    }

}

void test_file_replace_text() {
    for (size_t i = 0; i < sizeof(targets) / sizeof(*targets); i += 2) {
        const char *filename = "test_file_replace_text.txt";
        const char *target = targets[i];
        const char *expected = targets[i + 1];
        FILE *fp;

        fp = fopen(filename, "w");
        if (fp) {
            fprintf(fp, "%s", test_string);
            fclose(fp);
            OMC_ASSERT(file_replace_text(filename, target, "^^^", 0) == 0, "string replacement failed");
        } else {
            OMC_ASSERT(false, "failed to open file for writing");
            return;
        }

        char input[BUFSIZ] = {0};
        fp = fopen(filename, "r");
        if (fp) {
            fread(input, sizeof(*input), sizeof(input), fp);
            OMC_ASSERT(strcmp(input, expected) == 0, "unexpected replacement");
        } else {
            OMC_ASSERT(false, "failed to open file for reading");
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    OMC_TEST_BEGIN_MAIN();
    OMC_TEST_FUNC *tests[] = {
        test_replace_text,
        test_file_replace_text,
    };
    OMC_TEST_RUN(tests);
    OMC_TEST_END_MAIN();
}