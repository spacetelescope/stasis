#ifndef STASIS_ARGS_H
#define STASIS_ARGS_H

#include <getopt.h>

#define OPT_MICROMAMBA_DOWNLOAD_URL 1000

extern struct option long_options[];
void usage(char *name);

#endif //STASIS_ARGS_H
