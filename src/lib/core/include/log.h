#ifndef STASIS_EXECPOINT_H
#define STASIS_EXECPOINT_H

#include <stdio.h>

enum LogLevel {
    LOG_LEVEL_WARN = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
};
extern enum LogLevel LOG_LEVEL;

struct ExecPoint {
    const int line;  // line number
    const char *file;  // file name of origin
    const char *function;  // function of origin
};

#define EXECPOINT (struct ExecPoint) {.line = __LINE__, .file = __FILE__, .function = __func__}

void log_print_error(struct ExecPoint ep, const char *fmt, ...);
void log_print_warning(struct ExecPoint ep, const char *fmt, ...);
void log_print_info(struct ExecPoint ep, const char *fmt, ...);
void log_print_debug(struct ExecPoint ep, const char *fmt, ...);
int log_msgv(FILE *stream, struct ExecPoint ep, const char *preface_color, const char *preface, const char *fmt, va_list ap);
int log_msg(FILE *stream, struct ExecPoint ep, const char *preface_color, const char *preface, const char *fmt, ...);
const char *log_get_level_str(void);

#endif // STASIS_EXECPOINT_H
