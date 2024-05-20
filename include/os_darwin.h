#ifndef OMC_OS_DARWIN_H
#define OMC_OS_DARWIN_H

#include <sys/mount.h>
#include <sys/utsname.h>

#ifndef __DARWIN_64_BIT_INO_T
#define statvfs statfs

#ifndef ST_RDONLY
#define ST_RDONLY MNT_RDONLY
#endif

#define ST_NOEXEC MNT_NOEXEC
#define f_flag f_flags
#endif // __DARWIN_64_BIT_INO_T

#include <limits.h>

#ifndef PATH_MAX
#include <sys/syslimits.h>
#endif

extern char **environ;
#define __environ environ

#endif
