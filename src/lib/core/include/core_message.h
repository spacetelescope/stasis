
#ifndef STASIS_CORE_MESSAGE_H
#define STASIS_CORE_MESSAGE_H

#define SYSERROR(FMT, ...) do { \
    log_print_error(EXECPOINT, (FMT), ##__VA_ARGS__); \
} while (0)

#define SYSWARN(FMT, ...) do { \
    log_print_warning(EXECPOINT, (FMT), ##__VA_ARGS__); \
} while (0)

#define SYSINFO(FMT, ...) do { \
    log_print_info(EXECPOINT, (FMT), ##__VA_ARGS__); \
} while (0)

#define SYSDEBUG(FMT, ...) do { \
    log_print_debug(EXECPOINT, (FMT), ##__VA_ARGS__); \
} while (0)

#endif //STASIS_CORE_MESSAGE_H
