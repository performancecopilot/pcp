#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../config-reader/config-reader.h"

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

#define DURATION_METRIC "ms"
#define COUNTER_METRIC "c"
#define GAUGE_METRIC "g"

/**
 * Flag used to determine if VERBOSE output is allowed to be printed
 */
static int g_verbose_flag = 0;

/**
 * Flag used to determine if TRACE output is allowed to be printed
 */
static int g_trace_flag = 0;

/**
 * Flag used to determine if DEBUG output is allowed to be printed
 */
static int g_debug_flag = 0;

/**
 * Kills application with given message
 * @arg filename - Current filename
 * @arg line_number - Current line number
 * @arg format - Format string
 * @arg ... - variables to print
 */
void die(char* filename, int line_number, const char* format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    fprintf(stderr, "%s@%d: ", filename, line_number);
    vfprintf(stderr, format, vargs);
    fprintf(stderr, "\n");
    va_end(vargs);
    exit(1);
}

/**
 * Prints warning message
 * @arg filename - Current filename
 * @arg line_number - Current line number
 * @arg format - Format string
 * @arg ... - variables to print
 */
void warn(char* filename, int line_number, const char* format, ...) {
    va_list vargs;
    va_start(vargs, format);
    fprintf(stderr, YEL "WARNING on line %s@%d: " RESET, filename, line_number);
    vfprintf(stderr, format, vargs);
    fprintf(stderr, "\n");
    va_end(vargs);
}

/**
 * Sanitizes string
 * Swaps '/', '-', ' ' characters with '-'. Should the message contain any other characters then a-z, A-Z, 0-9 and specified above, fails.
 * @arg src - String to be sanitized
 * @return 1 on success
 */
int sanitize_string(char *src) {
    int segment_length = strlen(src);
    if (segment_length == 0) {
        return 0;
    }
    int i;
    for (i = 0; i < segment_length; i++) {
        char current_char = src[i];
        if (((int) current_char >= (int) 'a' && (int) current_char <= (int) 'z') ||
            ((int) current_char >= (int) 'A' && (int) current_char <= (int) 'Z') ||
            ((int) current_char >= (int) '0' && (int) current_char <= (int) '9') ||
            (int) current_char == (int) '.' ||
            (int) current_char == (int) '_') {
            continue;
        } else if ((int) current_char == (int) '/' || 
                 (int) current_char == (int) '-' ||
                 (int) current_char == (int) ' ') {
            src[i] = '_';
        } else {
            return 0;
        }
    }
    return 1;
}

/**
 * Validates string
 * Checks if there are any non numerical characters (0-9), excluding '+' and '-' on first position and is not empty.
 * @arg src - String to be validated
 * @return 1 on success
 */
int sanitize_metric_val_string(char* src) {
    int segment_length = strlen(src);
    if (segment_length == 0) {
        return 0;
    }
    int i;
    for (i = 0; i < segment_length; i++) {
        char current_char = src[i];
        if (i == 0) {
            if (((int) current_char >= (int) '0' && (int) current_char <= (int) '9') ||
                (current_char == '+') ||
                (current_char == '-')) {
                continue;
            } else {
                return 0;
            }
        } else {
            if (((int) current_char >= (int) '0' && (int) current_char <= (int) '9') || current_char == '.') {
                continue;
            } else {
                return 0;
            }
        }
    }
    return 1;
}

/**
 * Validates string
 * Checks if string is convertible to double and is not empty.
 * @arg src - String to be validated
 * @return 1 on success
 */
int sanitize_sampling_val_string(char* src) {
    int segment_length = strlen(src);
    if (segment_length == 0) {
        return 0;
    }
    char* end;
    strtod(src, &end);
    if (src == end || *end != '\0') {
        return 0;
    }
    return 1;
}

/**
 * Validates string
 * Checks if string is matching one of metric identifiers ("ms" = duration, "g" = gauge, "c" = counter)
 * @arg src - String to be validated
 * @return 1 on success
 */
int sanitize_type_val_string(char* src) {
    if (strcmp(src, GAUGE_METRIC) == 0 ||
        strcmp(src, COUNTER_METRIC) == 0 ||
        strcmp(src, DURATION_METRIC) == 0) {
            return 1;
        }
    return 0;
}

/**
 * Logs VERBOSE message - if config settings allows it
 * @arg format - Format string
 * @arg ... - variables to print
 */
void verbose_log(const char* format, ...) {
    if (g_verbose_flag) {
        va_list vargs;
        va_start(vargs, format);
        fprintf(stdout, YEL "VERBOSE LOG: " RESET);
        vfprintf(stdout, format, vargs);
        fprintf(stdout, "\n");
        va_end(vargs);
    }
}

/**
 * Logs DEBUG message - if config settings allows it
 * @arg format - Format string
 * @arg ... - variables to print
 */
void debug_log(const char* format, ...) {
    if (g_debug_flag) {
        va_list vargs;
        va_start(vargs, format);
        fprintf(stdout, MAG "DEBUG LOG: " RESET);
        vfprintf(stdout, format, vargs);
        fprintf(stdout, "\n");
        va_end(vargs);
    }
}

/**
 * Logs TRACE message - if config settings allows it
 * @arg format - Format string
 * @arg ... - variables to print
 */
void trace_log(const char* format, ...) {
    if (g_trace_flag) {
        va_list vargs;
        va_start(vargs, format);
        fprintf(stdout, CYN "TRACE LOG: " RESET);
        vfprintf(stdout, format, vargs);
        fprintf(stdout, "\n");
        va_end(vargs);
    }
}

/**
 * Initializes debugging/verbose/tracing flags based on given config
 * @arg config - Config to check against
 */
void init_loggers(agent_config* config) {
    g_verbose_flag = config->verbose;
    g_trace_flag = config->trace;
    g_debug_flag = config->debug;
}
