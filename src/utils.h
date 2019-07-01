#ifndef UTILS_
#define UTILS_

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "config-reader.h"

/**
 * Checks if last allocation was OK 
 */
#define ALLOC_CHECK(desc, ...) \
    if (errno == ENOMEM) { \
        DIE(desc, ## __VA_ARGS__); \
    } \

/**
 * Checks if thread was OK
 */
#define PTHREAD_CHECK(ret) \
    if (ret != 0) { \
        if (ret == EAGAIN) { \
            DIE("Insufficient resources to create another thread."); \
        } \
        if (ret == EINVAL) { \
            DIE("Invalid settings in attr."); \
        } \
        if (ret == EPERM) { \
            DIE("No permission to set the scheduling policy and parameters specified in attr."); \
        } \
    } \

/**
 * Exists program
 */
#define DIE(format, ...)  die(__FILE__, __LINE__, format, ## __VA_ARGS__)

/**
 * Prints warning message
 */
#define WARN(format, ...) warn(__FILE__, __LINE__, format, ## __VA_ARGS__)

/**
 * Kills application with given message
 * @arg line_number - Current line number
 * @arg format - Format string
 * @arg ... - variables to print
 */
void
die(char* filename, int line_number, const char* format, ...);

/**
 * Prints warning message
 * @arg line_number - Current line number
 * @arg format - Format string
 * @arg ... - variables to print
 */
void
warn(char* filename, int line_number, const char* format, ...);

/**
 * Sanitizes string
 * Swaps '/', '-', ' ' characters with '-'. Should the message contain any other characters then a-z, A-Z, 0-9 and specified above, fails.
 * @arg src - String to be sanitized
 * @return 1 on success
 */
int
sanitize_string(char* src, size_t num);

/**
 * Validates string
 * Checks if there are any non numerical characters (0-9), excluding '+' and '-' on first position and is not empty.
 * @arg src - String to be validated
 * @return 1 on success
 */
int
sanitize_metric_val_string(char* src);

/**
 * Validates string
 * Checks if string is convertible to double and is not empty.
 * @arg src - String to be validated
 * @return 1 on success
 */
int
sanitize_sampling_val_string(char* src);

/**
 * Validates string
 * Checks if string is matching one of metric identifiers ("ms" = duration, "g" = gauge, "c" = counter)
 * @arg src - String to be validated
 * @return 1 on success
 */
int
sanitize_type_val_string(char* src);

/**
 * Logs VERBOSE message - if config settings allows it
 * @arg format - Format string
 * @arg ... - variables to print
 */
void
verbose_log(const char* format, ...);

/**
 * Logs DEBUG message - if config settings allows it
 * @arg format - Format string
 * @arg ... - variables to print
 */
void
debug_log(const char* format, ...);

/**
 * Logs TRACE message - if config settings allows it
 * @arg format - Format string
 * @arg ... - variables to print
 */
void
trace_log(const char* format, ...);

/**
 * Initializes debugging/verbose/tracing flags based on given config
 * @arg config - Config to check against
 */
void
init_loggers(struct agent_config* config);

#endif
