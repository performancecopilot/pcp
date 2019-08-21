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

#define CHECK_ERROR(string, name, tags, value, type, explicit_sign) \
    fprintf(stdout, MAG "CASE: %s " RESET "\n", string); \
    if (parse(string, datagram)) { \
        int local_err = 0; \
        local_err += assert_statsd_datagram_eq(datagram, name, tags, value, type, explicit_sign); \
        error_count += local_err; \
    } else { \
        if (name != NULL || tags != NULL || value != 0 || type != METRIC_TYPE_NONE || explicit_sign != SIGN_NONE) { \
            fprintf(stdout, RED "ERROR: " RESET "Should have failed parsing. \n"); \
            error_count += 1; \
        } \
    } \

#define INIT_TEST(name, fn) \
    struct timeval t0, t1; \
    fprintf(stdout, YEL name RESET "\n"); \
    long int error_count = 0; \
    struct statsd_datagram** datagram = (struct statsd_datagram**) malloc(sizeof(struct statsd_datagram*)); \
    *datagram = (struct statsd_datagram*) malloc(sizeof(struct statsd_datagram)); \
    int (*parse)(char*, struct statsd_datagram**); \
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

struct tag {
    char* key;
    char* value;
} tag;

struct tag_collection {
    struct tag** values;
    size_t length;
} tag_collection;

/**
 * Converts tag_collection* struct to JSON string that is sorted by keys and contains no duplicities (right-most wins) 
 */
char*
tag_collection_to_json(struct tag_collection* tags);

void
free_tag_collection(struct tag_collection* tags);

int
assert_statsd_datagram_eq(
    struct statsd_datagram** datagram,
    char* name,
    char* tags,
    double value,
    enum METRIC_TYPE type,
    enum SIGN explicit_sign
);

#endif
