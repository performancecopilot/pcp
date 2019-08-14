#ifndef UTILS_
#define UTILS_

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#include "aggregator-metrics.h"
#include "config-reader.h"

#define VERBOSE_LOG(format, ...) \
    if (is_verbose()) { \
        pmNotifyErr(LOG_INFO, format, ## __VA_ARGS__); \
    } \

#define DEBUG_LOG(format, ...) \
    if (is_debug()) { \
        pmNotifyErr(LOG_DEBUG, format, ## __VA_ARGS__); \
    } \

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
#define DIE(format, ...) \
    pmNotifyErr(LOG_ALERT, format, ## __VA_ARGS__);

/**
 * Prints warning message
 */
#define WARN(format, ...) \
    pmNotifyErr(LOG_WARNING, format, ## __VA_ARGS__);

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
 * Validates type string
 * Checks if string is matching one of metric identifiers ("ms" = duration, "g" = gauge, "c" = counter)
 * @arg src - String to be validated
 * @arg out - What metric string contained
 * @return 1 on success
 */
int
sanitize_type_val_string(char* src, enum METRIC_TYPE* out);

/**
 * Check *verbose* flag
 * @return verbose flag
 */
int
is_verbose();

/**
 * Check *debug* flag
 * @return debug flag
 */
int
is_debug();

/**
 * Initializes debugging/verbose/tracing flags based on given config
 * @arg config - Config to check against
 */
void
init_loggers(struct agent_config* config);

#endif
