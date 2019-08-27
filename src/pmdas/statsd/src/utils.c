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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include "config-reader.h"
#include "parsers.h"
#include "utils.h"

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
 * Used to prevent racing between pmErrLog function calls in different threads 
 */
static pthread_mutex_t g_log_mutex;

/**
 * Used to signal threads that its time to go home
 */
static int g_exit_flag = 0;

/**
 * Verbosity level
 */
static int g_verbosity = 0;

/**
 * Metric processing error counter,
 * in non verbose=2 serves as maximum for count of shown metric processing errors
 */
static long unsigned int g_metric_error_counter = 0; 

/**
 * Metric error count threshold
 */
static long unsigned int g_metric_error_threshold = 1000;

/**
 * Validates valid metric name string
 * Checks if string starts with [a-zA-Z] and that rest is [a-zA-Z0-9._]
 * @arg src - String to be sanitized
 * @arg num - Boundary
 * @return 1 on success
 */
int
validate_metric_name_string(char* src, size_t num) {
    size_t segment_length = strlen(src);
    if (segment_length == 0) {
        return 0;
    }
    if (segment_length > num) {
        segment_length = num;
    }
    size_t i;
    for (i = 0; i < segment_length; i++) {
        char current_char = src[i];
        if (i == 0) {
            if (((int) current_char >= (int) 'a' && (int) current_char <= (int) 'z') ||
                ((int) current_char >= (int) 'A' && (int) current_char <= (int) 'Z')) {
                continue;
            } else {
                return 0;
            }
        }
        if (((int) current_char >= (int) 'a' && (int) current_char <= (int) 'z') ||
            ((int) current_char >= (int) 'A' && (int) current_char <= (int) 'Z') ||
            ((int) current_char >= (int) '0' && (int) current_char <= (int) '9') ||
            (int) current_char == (int) '.' ||
            (int) current_char == (int) '_') {
            continue;
        } else {
            return 0;
        }
    }
    return 1;
}

/**
 * Sanitizes string
 * Swaps '/', '-', ' ' characters with '_'. Should the message contain any other characters then a-z, A-Z, 0-9 and specified above, fails. 
 * First character needs to be in a-zA-Z
 * @arg src - String to be sanitized
 * @arg num - Boundary
 * @return 1 on success
 */
int
sanitize_string(char* src, size_t num) {
    size_t segment_length = strlen(src);
    if (segment_length == 0) {
        return 0;
    }
    if (segment_length > num) {
        segment_length = num;
    }
    size_t i;
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
 * Validates metric val string
 * Checks if there are any non numerical characters (0-9), excluding '+' and '-' on first position and is not empty.
 * @arg src - String to be validated
 * @return 1 on success
 */
int
sanitize_metric_val_string(char* src) {
    size_t segment_length = strlen(src);
    if (segment_length == 0) {
        return 0;
    }
    size_t i;
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
 * Validates type string
 * Checks if string is matching one of metric identifiers ("ms" = duration, "g" = gauge, "c" = counter)
 * @arg src - String to be validated
 * @arg out - What metric string contained
 * @return 1 on success
 */
int
sanitize_type_val_string(char* src, enum METRIC_TYPE* out) {
    if (strcmp(src, GAUGE_METRIC) == 0) {
        *out = METRIC_TYPE_GAUGE;
        return 1;
    } else if (strcmp(src, COUNTER_METRIC) == 0) {
        *out = METRIC_TYPE_COUNTER;
        return 1;
    } else if (strcmp(src, DURATION_METRIC) == 0) {
        *out = METRIC_TYPE_DURATION;
        return 1;
    } else {
        return 0;
    }
}

/**
 * Check *verbose* flag
 * @return 1 if below or equal, else 0
 */
int 
check_verbosity(int level) {
    if (level <= g_verbosity) {
        return 1;
    }
    return 0;
}

/**
 * Checks that error count for metrics is below threshold
 * @return if passes
 */
int
is_metric_err_below_threshold() {
    if (g_metric_error_counter < g_metric_error_threshold) {
        return 1;
    }
    return 0;
}

/**
 * Increments error count for metrics
 */
void
increment_metric_err_count() {
    __sync_add_and_fetch(&g_metric_error_counter, 1);
}

/**
 * Prints error message about reaching metric error message count threshold - only once, subsequent calls dont do anything.
 */
void
maybe_print_metric_err_msg() {
    static int i = 0;
    if (i == 0) {
        pmNotifyErr(LOG_ERR, "Too many dropped messages, ignoring until next restart.");
        i++;
    }
}

void
log_mutex_lock() {
    pthread_mutex_lock(&g_log_mutex);
}

void
log_mutex_unlock() {
    pthread_mutex_unlock(&g_log_mutex);
}

void
set_exit_flag() {
    __sync_add_and_fetch(&g_exit_flag, 1);
}

int
check_exit_flag() {
    return g_exit_flag;
}

/**
 * Initializes debugging/verbose/tracing flags based on given config
 * @arg config - Config to check against
 */
void
init_loggers(struct agent_config* config) {
    g_verbosity = config->verbose;
}
