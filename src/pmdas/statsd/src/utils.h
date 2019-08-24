/*
 * Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#ifndef UTILS_
#define UTILS_

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#include "aggregator-metrics.h"
#include "config-reader.h"

#define VERBOSE_LOG(level, format, ...) \
    if (check_verbosity(level)) { \
        log_mutex_lock(); \
        pmNotifyErr(LOG_INFO, format, ## __VA_ARGS__); \
        log_mutex_unlock(); \
    } \

// There may be small race condition here,
// but an issue of printing few more errors then is needed is not critical enough to create mutex.
/**
 * Logs metric processing related error. Ignores any after such error count passes threshold. In verbose=2 no errors are ignored.
 */
#define METRIC_PROCESSING_ERR_LOG(format, ...) \
    log_mutex_lock(); \
    if (is_metric_err_below_threshold()) { \
        pmNotifyErr(LOG_ERR, format, ## __VA_ARGS__); \
        if (!check_verbosity(2)) { \
            increment_metric_err_count(); \
        } \
    } else { \
        maybe_print_metric_err_msg(); \
    } \
    log_mutex_unlock(); \

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
 * Exits program, with a message
 */
#define DIE(format, ...) \
    log_mutex_lock(); \
    pmNotifyErr(LOG_ALERT, format, ## __VA_ARGS__); \
    log_mutex_unlock(); \
    exit(1) \

/**
 * Validates valid metric name string
 * Checks if string starts with [a-zA-Z] and that rest is [a-zA-Z0-9._]
 * @arg src - String to be sanitized
 * @arg num - Boundary
 * @return 1 on success
 */
int
validate_metric_name_string(char* src, size_t num);

/**
 * Sanitizes string
 * Swaps '/', '-', ' ' characters with '_'. Should the message contain any other characters then a-z, A-Z, 0-9 and specified above, fails. 
 * First character needs to be in a-zA-Z
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
 * @return 1 if below or equal, else 0
 */
int 
check_verbosity(int level);

/**
 * Checks that error count for metrics is below threshold
 * @return if passes
 */
int
is_metric_err_below_threshold();

/**
 * Increments error count for metrics
 */
void
increment_metric_err_count();

/**
 * Prints error message about reaching metric error message count threshold - only once, subsequent calls dont do anything.
 */
void
maybe_print_metric_err_msg();

/**
 * Check *debug* flag
 * @return debug flag
 */
int
is_debug();

void
log_mutex_lock();

void
log_mutex_unlock();

void
set_exit_flag();

int
check_exit_flag();

/**
 * Initializes debugging/verbose/tracing flags based on given config
 * @arg config - Config to check against
 */
void
init_loggers(struct agent_config* config);

#endif
