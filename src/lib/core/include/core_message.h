
#ifndef STASIS_CORE_MESSAGE_H
#define STASIS_CORE_MESSAGE_H

#define SYSERROR(MSG, ...) do { \
    fprintf(stderr, "%s:%d:%s():%s - ", path_basename(__FILE__), __LINE__, __FUNCTION__, (errno > 0) ? strerror(errno) : "info"); \
    fprintf(stderr, MSG LINE_SEP, __VA_ARGS__); \
} while (0)

#ifdef DEBUG
#define SYSDEBUG(MSG, ...) do { \
    fprintf(stderr, "DEBUG: %s:%d:%s(): ", path_basename(__FILE__), __LINE__, __FUNCTION__); \
    fprintf(stderr, MSG LINE_SEP, __VA_ARGS__); \
} while (0)
#else
#define SYSDEBUG(MSG, ...) do {} while (0)
#endif

#endif //STASIS_CORE_MESSAGE_H
