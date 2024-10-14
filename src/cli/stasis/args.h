#ifndef STASIS_ARGS_H
#define STASIS_ARGS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define OPT_ALWAYS_UPDATE_BASE 1000
#define OPT_NO_DOCKER 1001
#define OPT_NO_ARTIFACTORY 1002
#define OPT_NO_ARTIFACTORY_BUILD_INFO 1003
#define OPT_NO_TESTING 1004
#define OPT_OVERWRITE 1005
#define OPT_NO_REWRITE_SPEC_STAGE_2 1006
#define OPT_FAIL_FAST 1007
#define OPT_NO_PARALLEL 1008
#define OPT_POOL_STATUS_INTERVAL 1009

extern struct option long_options[];
void usage(char *progname);

#endif //STASIS_ARGS_H
