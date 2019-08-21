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
#include <pcp/pmapi.h>

#include "parsers-utils.h"
#include "parsers.h"
#include "string.h"
#include "utils.h"
#include "aggregator-metrics.h"

#define RED   "\x1B[31m"
#define RESET "\x1B[0m"

#define CHECK_DISCREPANCY(field, string)    (field != NULL && string != NULL && strcmp(field, string) != 0) || \
                                            (field != NULL && string == NULL) || \
                                            (field == NULL && string != NULL) \

#define CHECK_DISCREPANCY_VALUE(field, value) (field != value)

static int
tag_comparator(const void* x, const void* y) {
    int res = strcmp((*(struct tag**)x)->key, (*(struct tag**)y)->key);
    return res;
}

/**
 * Converts tag_collection* struct to JSON string that is sorted by keys and contains no duplicities (right-most wins) 
 */
char*
tag_collection_to_json(struct tag_collection* tags) {
    char buffer[JSON_BUFFER_SIZE];
    qsort(tags->values, tags->length, sizeof(struct tag*), tag_comparator);
    buffer[0] = '{';
    size_t i;
    size_t current_size = 1;
    char* format_first = "\"%s\":\"%s\"";
    char* format_rest = ",\"%s\":\"%s\"";
    int first_tag = 1;
    for (i = 0; i < tags->length; i++) {
        struct tag* current_tag = tags->values[i];
        if (i + 1 < tags->length) {
            struct tag* next_tag = tags->values[i + 1];
            if (strcmp(next_tag->key, current_tag->key) == 0) {
                continue;
            } 
        }
        int pair_length = 
            pmsprintf(
                buffer + current_size,
                JSON_BUFFER_SIZE - current_size,
                first_tag ? format_first : format_rest,
                current_tag->key,
                current_tag->value
            );
        current_size += pair_length;
        first_tag = 0;
    }
    if (current_size >= JSON_BUFFER_SIZE - 2) {
        return NULL;
    }
    buffer[current_size] = '}';
    buffer[current_size + 1] = '\0';
    char* result = malloc(sizeof(char) * (current_size + 2));
    ALLOC_CHECK("Unable to allocate memory for tags json.");
    memcpy(result, buffer, current_size + 2);
    return result;
}

void
free_tag_collection(struct tag_collection* tags) {
    if (tags != NULL) {
        size_t i;
        for (i = 0; i < tags->length; i++) {
            struct tag* t = tags->values[i]; 
            if (t != NULL) {
                if (t->key != NULL) {
                    free(t->key);
                }
                if (t->value != NULL) {
                    free(t->value);
                }
                free(t);
            }
        }
        free(tags->values);
        free(tags);
    }
}

static const char*
metric_enum_to_str(enum METRIC_TYPE type) {
    const char* counter = "counter";
    const char* gauge = "gauge";
    const char* duration = "duration";
    switch(type) {
        case METRIC_TYPE_COUNTER:
            return counter;
        case METRIC_TYPE_GAUGE:
            return gauge;
        case METRIC_TYPE_DURATION:
            return duration;
        default:
            return NULL;
    }
}

static const char*
sign_enum_to_str(enum SIGN sign) {
    const char* plus = "counter";
    const char* minus = "gauge";
    const char* none = "duration";
    switch(sign) {
        case SIGN_NONE:
            return none;
        case SIGN_MINUS:
            return minus;
        case SIGN_PLUS:
            return plus;
        default:
            return NULL;
    }
}

int
assert_statsd_datagram_eq(
    struct statsd_datagram** datagram,
    char* name,
    char* tags,
    double value,
    enum METRIC_TYPE type,
    enum SIGN explicit_sign
) {
    long int err_count = 0;
    if (CHECK_DISCREPANCY((*datagram)->name, name)) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Metric name doesn't match! %s =/= %s \n", (*datagram)->name, name);
    }
    if (CHECK_DISCREPANCY((*datagram)->tags, tags)) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Tags don't match! %s =/= %s \n", (*datagram)->tags, tags);
    }
    if (CHECK_DISCREPANCY_VALUE((*datagram)->value, value)) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Value doesn't match! %f =/= %f \n", (*datagram)->value, value);
    }
    if (CHECK_DISCREPANCY_VALUE((*datagram)->type, type)) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Type doesn't match! %s =/= %s \n", metric_enum_to_str((*datagram)->type), metric_enum_to_str(type));
    }
    if ((*datagram)->explicit_sign != explicit_sign) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Sign doesn't match %s =/= %s \n", sign_enum_to_str((*datagram)->explicit_sign), sign_enum_to_str(explicit_sign));
    }
    return err_count;
}
