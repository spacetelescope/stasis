#ifndef STASIS_WHEEL_H
#define STASIS_WHEEL_H

#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include "str.h"
#define WHEEL_MATCH_EXACT 0 ///< Match when all patterns are present
#define WHEEL_MATCH_ANY 1 ///< Match when any patterns are present

struct Wheel {
    char *distribution; ///< Package name
    char *version; ///< Package version
    char *build_tag; ///< Package build tag (optional)
    char *python_tag; ///< Package Python tag (pyXY)
    char *abi_tag; ///< Package ABI tag (cpXY, abiX, none)
    char *platform_tag; ///< Package platform tag (linux_x86_64, any)
    char *path_name; ///< Path to package on-disk
    char *file_name; ///< Name of package on-disk
};

/**
 * Extract metadata from a Python Wheel file name
 *
 * @param basepath directory containing a wheel file
 * @param name of wheel file
 * @param to_match a NULL terminated array of patterns (i.e. platform, arch, version, etc)
 * @param match_mode WHEEL_MATCH_EXACT
 * @param match_mode WHEEL_MATCH ANY
 * @return pointer to populated Wheel on success
 * @return NULL on error
 */
struct Wheel *get_wheel_info(const char *basepath, const char *name, char *to_match[], unsigned match_mode);
void wheel_free(struct Wheel **wheel);
#endif //STASIS_WHEEL_H
