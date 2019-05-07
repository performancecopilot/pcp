#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "../config-reader/config-reader.h"

#ifndef UTILS_
#define UTILS_

#define ALLOC_CHECK(desc, ...) \
    if (errno == ENOMEM) { \
        die(__LINE__, desc, ## __VA_ARGS__); \
    } \

// M/C/R-alloc can still return NULL when the address space is full.
#define PTHREAD_CHECK(ret) \
    if (ret != 0) { \
        if (ret == EAGAIN) { \
            die(__LINE__, "Insufficient resources to create another thread."); \
        } \
        if (ret == EINVAL) { \
            die(__LINE__, "Invalid settings in attr."); \
        } \
        if (ret == EPERM) { \
            die(__LINE__, "No permission to set the scheduling policy and parameters specified in attr."); \
        } \
    } \

void die(int line_number, const char* format, ...);

void warn(int line_number, const char* format, ...);

void sanitize_string(char *src);

void init_loggers(agent_config* config);

void verbose_log(const char* format, ...);

void debug_log(const char* format, ...);

void trace_log(const char* format, ...);

#endif