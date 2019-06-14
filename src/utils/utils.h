#ifndef UTILS_
#define UTILS_

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "../config-reader/config-reader.h"

/**
 * Checks if last allocation was OK 
 */
#define ALLOC_CHECK(desc, ...) \
    if (errno == ENOMEM) { \
        die(__FILE__, __LINE__, desc, ## __VA_ARGS__); \
    } \

// M/C/R-alloc can still return NULL when the address space is full.
#define PTHREAD_CHECK(ret) \
    if (ret != 0) { \
        if (ret == EAGAIN) { \
            die(__FILE__, __LINE__, "Insufficient resources to create another thread."); \
        } \
        if (ret == EINVAL) { \
            die(__FILE__, __LINE__, "Invalid settings in attr."); \
        } \
        if (ret == EPERM) { \
            die(__FILE__, __LINE__, "No permission to set the scheduling policy and parameters specified in attr."); \
        } \
    } \

/**
 * Kills application with given message
 * @arg line_number - Current line number
 * @arg format - Format string
 * @arg ... - variables to print
 */
void die(char* filename, int line_number, const char* format, ...);

/**
 * Prints warning message
 * @arg line_number - Current line number
 * @arg format - Format string
 * @arg ... - variables to print
 */
void warn(char* filename, int line_number, const char* format, ...);

/**
 * Sanitizes string
 * Swaps '/', '-', ' ' characters with '-'. Should the message contain any other characters then a-z, A-Z, 0-9 and specified above, fails.
 * @arg src - String to be sanitized
 * @return 1 on success
 */
int sanitize_string(char* src);

/**
 * Validates string
 * Checks if there are any non numerical characters (0-9), excluding '+' and '-' on first position and is not empty.
 * @arg src - String to be validated
 * @return 1 on success
 */
int sanitize_metric_val_string(char* src);

/**
 * Validates string
 * Checks if string is convertible to double and is not empty.
 * @arg src - String to be validated
 * @return 1 on success
 */
int sanitize_sampling_val_string(char* src);

/**
 * Validates string
 * Checks if string is matching one of metric identifiers ("ms" = duration, "g" = gauge, "c" = counter)
 * @arg src - String to be validated
 * @return 1 on success
 */
int sanitize_type_val_string(char* src);

/**
 * Logs VERBOSE message - if config settings allows it
 * @arg format - Format string
 * @arg ... - variables to print
 */
void verbose_log(const char* format, ...);

/**
 * Logs DEBUG message - if config settings allows it
 * @arg format - Format string
 * @arg ... - variables to print
 */
void debug_log(const char* format, ...);

/**
 * Logs TRACE message - if config settings allows it
 * @arg format - Format string
 * @arg ... - variables to print
 */
void trace_log(const char* format, ...);

/**
 * Initializes debugging/verbose/tracing flags based on given config
 * @arg config - Config to check against
 */
void init_loggers(agent_config* config);

#endif
