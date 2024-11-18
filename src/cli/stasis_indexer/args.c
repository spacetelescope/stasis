#include "core.h"
#include "args.h"

struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"destdir", required_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"unbuffered", no_argument, 0, 'U'},
    {"web", no_argument, 0, 'w'},
    {0, 0, 0, 0},
};

const char *long_options_help[] = {
    "Display this usage statement",
    "Destination directory",
    "Increase output verbosity",
    "Disable line buffering",
    "Generate HTML indexes (requires pandoc)",
    NULL,
};

void usage(char *name) {
    const int maxopts = sizeof(long_options) / sizeof(long_options[0]);
    char *opts = calloc(maxopts + 1, sizeof(char));
    for (int i = 0; i < maxopts; i++) {
        opts[i] = (char) long_options[i].val;
    }
    printf("usage: %s [-%s] {{STASIS_ROOT}...}\n", name, opts);
    guard_free(opts);

    for (int i = 0; i < maxopts - 1; i++) {
        char line[255] = {0};
        sprintf(line, "  --%s  -%c  %-20s", long_options[i].name, long_options[i].val, long_options_help[i]);
        puts(line);
    }

}

