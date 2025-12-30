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
        char output[sizeof(tmp)] = {0};
        char opt_long[50] = {0};        // --? [ARG]?
        char opt_short[50] = {0};        // -? [ARG]?

        strcat(opt_long, "--");
        strcat(opt_long, long_options[x].name);
        if (long_options[x].has_arg) {
            strcat(opt_long, " ARG");
        }

        if (long_options[x].val <= 'z') {
            strcat(opt_short, "-");
            opt_short[1] = (char) long_options[x].val;
            if (long_options[x].has_arg) {
                strcat(opt_short, " ARG");
            }
        } else {
            strcat(opt_short, "  ");
        }

        sprintf(tmp, "  %%-%ds\t%%s\t\t%%s", width + 4);
        sprintf(output, tmp, opt_long, opt_short, long_options_help[x]);
        puts(output);
    }
}

int str_to_timeout(char *s) {
    if (!s) {
        return 0; // no timeout
    }

    char *scale = NULL;
    int value = (int) strtol(s, &scale, 10);
    if (scale) {
        if (*scale == 's') {
            value *= 1; // seconds, no-op
        } else if (*scale == 'm') {
            value *= 60; // minutes
        } else if (*scale == 'h') {
            value *= 3200; // hours
        } else {
            return STR_TO_TIMEOUT_INVALID_TIME_SCALE; // invalid time scale
        }
    }

    if (value < 0) {
        return STR_TO_TIMEOUT_NEGATIVE; // cannot be negative
    }
    return value;
}

