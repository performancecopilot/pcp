#include <pcp/pmapi.h>

#include "parsers-utils.h"
#include "parsers.h"
#include "string.h"
#include "utils.h"

#define RED   "\x1B[31m"
#define RESET "\x1B[0m"

#define CHECK_DISCREPANCY(field, string)    (field != NULL && string != NULL && strcmp(field, string) != 0) || \
                                            (field != NULL && string == NULL) || \
                                            (field == NULL && string != NULL) \

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

int
assert_statsd_datagram_eq(
    struct statsd_datagram** datagram,
    char* metric,
    char* tags,
    char* instance,
    char* value,
    char* type,
    char* sampling) {
    long int err_count = 0;
    if (CHECK_DISCREPANCY((*datagram)->metric, metric)) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Metric name doesn't match! %s =/= %s \n", (*datagram)->metric, metric);
    }
    if (CHECK_DISCREPANCY((*datagram)->tags, tags)) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Tags don't match! %s =/= %s \n", (*datagram)->tags, tags);
    }
    if (CHECK_DISCREPANCY((*datagram)->instance, instance)) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Instance doesn't match! %s =/= %s \n", (*datagram)->instance, instance);
    }
    if (CHECK_DISCREPANCY((*datagram)->value, value)) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Value doesn't match! %s =/= %s \n", (*datagram)->value, value);
    }
    if (CHECK_DISCREPANCY((*datagram)->type, type)) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Type doesn't match! %s =/= %s \n", (*datagram)->type, type);
    }
    if (CHECK_DISCREPANCY((*datagram)->sampling, sampling)) {
        err_count++;
        fprintf(stdout, RED "FAIL: " RESET "Sampling doesn't match! %s =/= %s \n", (*datagram)->sampling, sampling);
    }
    return err_count;
}
