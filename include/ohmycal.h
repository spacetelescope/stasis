#ifndef OHMYCAL_OHMYCAL_H
#define OHMYCAL_OHMYCAL_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#define SYSERROR stderr, "%s:%s:%d: %s\n", path_basename(__FILE__), __FUNCTION__, __LINE__, strerror(errno)
#define OHMYCAL_BUFSIZ 8192

#include "utils.h"
#include "ini.h"
#include "conda.h"
#include "environment.h"
#include "deliverable.h"
#include "str.h"
#include "strlist.h"
#include "system.h"
#include "download.h"
#include "recipe.h"
#include "relocation.h"

#endif //OHMYCAL_OHMYCAL_H
