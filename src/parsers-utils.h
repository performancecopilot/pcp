#ifndef PARSERS_UTILS_
#define PARSERS_UTILS_

#include <sys/time.h>

#include "parsers.h"

#define JSON_BUFFER_SIZE 4096
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define RESET "\x1B[0m"

#define SUITE_HEADER(format, ...) fprintf(stdout, CYN format RESET "\n", ## __VA_ARGS__);

#define CHECK_ERROR(string, metric, tags, instance, value, type, sampling) \
    fprintf(stdout, MAG "CASE: %s " RESET "\n", string); \
    if (parse(string, datagram)) { \
        int local_err = 0; \
        local_err += assert_statsd_datagram_eq(datagram, metric, tags, instance, value, type, sampling); \
        error_count += local_err; \
    } else { \
        if (metric != NULL || tags != NULL || instance != NULL || value != NULL || type != NULL || sampling != NULL) { \
            fprintf(stdout, RED "ERROR: " RESET "Should have failed parsing. \n"); \
            error_count += 1; \
        } \
    } \

#define INIT_TEST(name, fn) \
    struct timeval t0, t1; \
    fprintf(stdout, YEL name RESET "\n"); \
    long int error_count = 0; \
    statsd_datagram** datagram = (struct statsd_datagram**) malloc(sizeof(struct statsd_datagram*)); \
    *datagram = (struct statsd_datagram*) malloc(sizeof(struct statsd_datagram)); \
    int (*parse)(char*, statsd_datagram**); \
    parse = &fn; \
    gettimeofday(&t0, NULL); \

#define END_TEST() \
    gettimeofday(&t1, NULL); \
    fprintf(stdout, "Completed in %ld microseconds.\n", t1.tv_usec - t0.tv_usec); \
    if (error_count == 0) { \
        fprintf(stdout, GRN "TEST PASSED. " RESET "0 errors.\n"); \
        return EXIT_SUCCESS; \
    } else { \
        fprintf(stdout, RED "TEST FAILED. " RESET "%ld errors.\n", error_count); \
        return EXIT_FAILURE; \
    } \

typedef struct tag {
    char* key;
    char* value;
} tag;

typedef struct tag_collection {
    tag** values;
    long int length;
} tag_collection;

/**
 * Converts tag_collection* struct to JSON string that is sorted by keys
 */
char* tag_collection_to_json(tag_collection* tags);

int assert_statsd_datagram_eq(statsd_datagram** datagram, char* metric, char* tags, char* instance, char* value, char* type, char* sampling);

#endif
