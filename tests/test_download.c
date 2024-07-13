#include "testing.h"

void test_download() {
    struct testcase {
        const char *url;
        long http_code;
        const char *data;
        const char *errmsg;
    };
    struct testcase tc[] = {
            {.url = "https://ssb.stsci.edu/jhunk/stasis_test/test_download.txt", .http_code = 200L, .data = "It works!\n", .errmsg = NULL},
            {.url = "https://ssb.stsci.edu/jhunk/stasis_test/test_download.broken", .http_code = 404L, .data = "<html", .errmsg = NULL},
            {.url = "https://example.tld", .http_code = -1L, .data = NULL, .errmsg = "Couldn't resolve host name"},
    };

    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        const char *filename = "output.txt";
        char errmsg[BUFSIZ] = {0};
        char *errmsg_p = errmsg;
        long http_code = download((char *) tc[i].url, filename, &errmsg_p);
        if (tc[i].errmsg) {
            STASIS_ASSERT(strlen(errmsg_p), "an error should have been thrown by curl, but wasn't");
            fflush(stderr);
            SYSERROR("curl error message: %s", errmsg_p);
        } else {
            STASIS_ASSERT(!strlen(errmsg_p), "unexpected error thrown by curl");
        }
        STASIS_ASSERT(http_code == tc[i].http_code, "expecting non-error HTTP code");

        char **data = file_readlines(filename, 0, 100, NULL);
        if (http_code >= 0) {
            STASIS_ASSERT(data != NULL, "data should not be null");
            STASIS_ASSERT(strncmp(data[0], tc[i].data, strlen(tc[i].data)) == 0, "data file does not match the expected contents");
        } else {
            STASIS_ASSERT(http_code == -1, "http_code should be -1 on fatal curl error");
            STASIS_ASSERT(data == NULL, "data should be NULL on fatal curl error");
        }
        guard_free(data);
        remove(filename);
    }
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
            test_download,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}