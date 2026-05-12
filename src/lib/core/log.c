#include "core.h"
#include "log.h"

enum LogLevel LOG_LEVEL = LOG_LEVEL_WARN;

void log_print_error(const struct ExecPoint ep, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_msgv(stderr, ep, STASIS_COLOR_RED, "ERROR", fmt, ap);
    va_end(ap);
}

void log_print_warning(const struct ExecPoint ep, const char *fmt, ...) {
    if (LOG_LEVEL < LOG_LEVEL_WARN) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    log_msgv(stderr, ep, STASIS_COLOR_YELLOW, "WARNING", fmt, ap);
    va_end(ap);
}

void log_print_info(const struct ExecPoint ep, const char *fmt, ...) {
    if (LOG_LEVEL < LOG_LEVEL_INFO) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    log_msgv(stdout, ep, STASIS_COLOR_WHITE, "INFO", fmt, ap);
    va_end(ap);
}

void log_print_debug(const struct ExecPoint ep, const char *fmt, ...) {
    if (LOG_LEVEL < LOG_LEVEL_DEBUG) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    log_msgv(stderr, ep, STASIS_COLOR_BLUE, "DEBUG", fmt, ap);
    va_end(ap);
}

int log_msgv(FILE *stream, const struct ExecPoint ep, const char *preface_color, const char *preface, const char *fmt, va_list ap) {
    char header[STASIS_BUFSIZ] = {0};
    const int no_info = LOG_LEVEL < LOG_LEVEL_INFO;
    const int some_info = LOG_LEVEL < LOG_LEVEL_DEBUG;

    int len = snprintf(header, sizeof(header),
        STASIS_COLOR_RESET
        "%s%s: "
        STASIS_COLOR_RESET,
        preface_color ? preface_color : STASIS_COLOR_RED,
        preface ? preface : "UNKNOWN");

    if (no_info) {
        len += snprintf(header + strlen(header), sizeof(header) - len,
            STASIS_COLOR_WHITE
            ""
            STASIS_COLOR_RESET);
    } else if (some_info) {
        len += snprintf(header + strlen(header), sizeof(header) - len,
            STASIS_COLOR_WHITE
            "%s(): "
            STASIS_COLOR_RESET,
            ep.function);
    } else {
        // everything
        len += snprintf(header + strlen(header), sizeof(header) - len,
            STASIS_COLOR_WHITE
            "%s:%d: %s(): "
            STASIS_COLOR_RESET,
            path_basename((char *) ep.file),
            ep.line,
            ep.function);
    }

    if (len >= (int) sizeof(header)) {
        SYSERROR("header format truncated (%d >= %d)", len, (int) sizeof(header));
        return len;
    }

    fprintf(stream, "%s", header);

    va_list ap_out;
    va_copy(ap_out, ap);
    len = vfprintf(stream, fmt ? fmt : "NO MESSAGE", ap_out);
    va_end(ap_out);
    if (len < 0) {
        SYSERROR("\nvfprintf failed");
        return len;
    }
    fprintf(stderr, LINE_SEP);
    return len;
}

int log_msg(FILE *stream, const struct ExecPoint ep, const char *preface_color, const char *preface, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int ret = log_msgv(stream, ep, preface_color, preface, fmt, ap);
    va_end(ap);
    return ret;
}

const char *log_get_level_str(void) {
    switch (LOG_LEVEL) {
        case LOG_LEVEL_WARN:
            return "WARN";
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_DEBUG:
        default:
            // Anything higher is still DEBUG
            return "DEBUG";
    }
}
