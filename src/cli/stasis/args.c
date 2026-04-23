#include "core.h"
#include "args.h"

struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"continue-on-error", no_argument, 0, 'C'},
    {"config", required_argument, 0, 'c'},
    {"cpu-limit", required_argument, 0, 'l'},
    {"pool-status-interval", required_argument, 0, OPT_POOL_STATUS_INTERVAL},
    {"python", required_argument, 0, 'p'},
    {"verbose", no_argument, 0, 'v'},
    {"unbuffered", no_argument, 0, 'U'},
    {"update-base", no_argument, 0, OPT_ALWAYS_UPDATE_BASE},
    {"fail-fast", no_argument, 0, OPT_FAIL_FAST},
    {"task-timeout", required_argument, 0, OPT_TASK_TIMEOUT},
    {"overwrite", no_argument, 0, OPT_OVERWRITE},
    {"wheel-builder", required_argument, 0, OPT_WHEEL_BUILDER},
    {"wheel-builder-manylinux-image", required_argument, 0, OPT_WHEEL_BUILDER_MANYLINUX_IMAGE},
    {"no-docker", no_argument, 0, OPT_NO_DOCKER},
    {"no-artifactory", no_argument, 0, OPT_NO_ARTIFACTORY},
    {"no-artifactory-build-info", no_argument, 0, OPT_NO_ARTIFACTORY_BUILD_INFO},
    {"no-artifactory-upload", no_argument, 0, OPT_NO_ARTIFACTORY_UPLOAD},
    {"no-testing", no_argument, 0, OPT_NO_TESTING},
    {"no-parallel", no_argument, 0, OPT_NO_PARALLEL},
    {"no-task-logging", no_argument, 0, OPT_NO_TASK_LOGGING},
    {"no-rewrite", no_argument, 0, OPT_NO_REWRITE_SPEC_STAGE_2},
    {0, 0, 0, 0},
};

const char *long_options_help[] = {
    "Display this usage statement",
    "Display program version",
    "Allow tests to fail",
    "Read configuration file",
    "Number of processes to spawn concurrently (default: cpus - 1)",
    "Report task status every n seconds (default: 30)",
    "Override version of Python in configuration",
    "Increase output verbosity",
    "Disable line buffering",
    "Update conda installation prior to STASIS environment creation",
    "On error, immediately terminate all tasks",
    "Terminate task after timeout is reached (#s, #m, #h)",
    "Overwrite an existing release",
    "Wheel building backend (build, cibuildwheel, manylinux)",
    "Manylinux image name",
    "Do not build docker images",
    "Do not upload artifacts to Artifactory",
    "Do not upload build info objects to Artifactory",
    "Do not upload artifacts to Artifactory (dry-run)",
    "Do not execute test scripts",
    "Do not execute tests in parallel",
    "Do not log task output (write to stdout)",
    "Do not rewrite paths and URLs in output files",
    NULL,
};

static int get_option_max_width(struct option option[]) {
    int i = 0;
    int max = 0;
    const int indent = 4;
    while (option[i].name != 0) {
        int len = (int) strlen(option[i].name);
        if (option[i].has_arg) {
            len += indent;
        }
        if (len > max) {
            max = len;
        }
        i++;
    }
    return max;
}

void usage(char *progname) {
    printf("usage: %s ", progname);
    printf("[-");
    for (int x = 0; long_options[x].val != 0; x++) {
        if (long_options[x].has_arg == no_argument && long_options[x].val <= 'z') {
            putchar(long_options[x].val);
        }
    }
    printf("] {DELIVERY_FILE}\n");

    int width = get_option_max_width(long_options);
    for (int x = 0; long_options[x].name != 0; x++) {
        char tmp[STASIS_NAME_MAX] = {0};
        char output[STASIS_NAME_MAX] = {0};
        char opt_long[50] = {0};        // --? [ARG]?
        char opt_short[50] = {0};        // -? [ARG]?

        strncat(opt_long, "--", sizeof(opt_long) - strlen(opt_long) - 1);
        strncat(opt_long, long_options[x].name, sizeof(opt_long) - strlen(opt_long) - 1);
        if (long_options[x].has_arg) {
            strncat(opt_long, " ARG", sizeof(opt_long) - strlen(opt_long) - 1);
        }

        if (long_options[x].val <= 'z') {
            strncat(opt_short, "-", sizeof(opt_short) - strlen(opt_short) - 1);
            opt_short[1] = (char) long_options[x].val;
            if (long_options[x].has_arg) {
                strncat(opt_short, " ARG", sizeof(opt_short) - strlen(opt_short) - 1);
            }
        } else {
            strncat(opt_short, "  ", sizeof(opt_short) - strlen(opt_short) - 1);
        }

        snprintf(tmp, sizeof(tmp) - strlen(tmp), "  %%-%ds\t%%s\t\t%%s", width + 4);
        snprintf(output, sizeof(output), tmp, opt_long, opt_short, long_options_help[x]);
        puts(output);
    }
}
