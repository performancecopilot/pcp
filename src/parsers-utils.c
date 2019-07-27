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
 * Converts tag_collection* struct to JSON string that is sorted by keys
 */
char*
tag_collection_to_json(struct tag_collection* tags) {
    char buffer[JSON_BUFFER_SIZE];
    qsort(tags->values, tags->length, sizeof(struct tag*), tag_comparator);
    buffer[0] = '{';
    size_t i;
    size_t current_size = 1;
    for (i = 0; i < tags->length; i++) {
        if (i == 0) {
            current_size += pmsprintf(buffer + current_size, JSON_BUFFER_SIZE - current_size, "\"%s\":\"%s\"",
                tags->values[i]->key, tags->values[i]->value);
        } else {
            current_size += pmsprintf(buffer + current_size, JSON_BUFFER_SIZE - current_size, ",\"%s\":\"%s\"",
                tags->values[i]->key, tags->values[i]->value);
        }
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

int
assert_statsd_datagram_eq(
    struct statsd_datagram** datagram,
    char* name,
    char* tags,
    double value,
    enum METRIC_TYPE type,
    double sampling
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
    if (CHECK_DISCREPANCY_VALUE((*datagram)->sampling, sampling)) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Sampling doesn't match! %f =/= %f \n", (*datagram)->sampling, sampling);
    }
    return err_count;
}
