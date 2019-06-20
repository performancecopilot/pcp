#include <hdr/hdr_histogram.h>
#include <string.h>

#include "config-reader.h"
#include "statsd-parsers.h"
#include "aggregators.h"
#include "aggregator-duration.h"
#include "aggregator-duration-exact.h"
#include "aggregator-duration-hdr.h"
#include "errno.h"

/**
 * Creates duration metric record of value subtype
 * @arg config - Config from in which duration type is specified
 * @arg datagram - Datagram with source data
 * @arg out - Placeholder metric
 * @return 1 on success, 0 on fail
 */
int create_duration_metric(agent_config* config, statsd_datagram* datagram, metric** out) {
    if (datagram->value[0] == '-' || datagram->value[0] == '+') {
        return 0;
    }
    long long unsigned int value = strtoull(datagram->value, NULL, 10);
    if (errno == ERANGE) {
        return 0;
    }
    if (config->duration_aggregation_type == DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM) {
        (*out)->value = create_hdr_duration_value(value);
    } else {
        (*out)->value = create_exact_duration_value(value);
    }
    (*out)->name = malloc(strlen(datagram->metric));
    strcpy((*out)->name, datagram->metric);
    (*out)->type = (METRIC_TYPE*) malloc(sizeof(METRIC_TYPE));
    *((*out)->type) = 4;
    (*out)->meta = create_metric_meta(datagram);
    return 1;
}

/**
 * Updates duration metric record of value subtype
 * @arg config - Config from which we know what duration type is, either HDR or exact
 * @arg item - Item to be updated
 * @arg datagram - Data to update the item with
 * @return 1 on success, 0 on fail
 */
int update_duration_metric(agent_config* config, metric* item, statsd_datagram* datagram) {
    if (datagram->value[0] == '-' || datagram->value[0] == '+') {
        return 0;
    }
    long long unsigned int value = strtoull(datagram->value, NULL, 10);
    if (errno == ERANGE) {
        return 0;
    }
    if (config->duration_aggregation_type == DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM) {
        update_hdr_duration_value((struct hdr_histogram*)item->value, value);
    } else {
        update_exact_duration_value((exact_duration_collection*)item->value, value);
    }
    return 1;
}

/**
 * Prints duration metric information
 * @arg config - Config where duration subtype is specified
 * @arg f - Opened file handle
 * @arg item - Metric to print out
 */
void print_duration_metric(agent_config* config, FILE* f, metric* item) {
    fprintf(f, "-----------------\n");
    fprintf(f, "name = %s\n", item->name);
    fprintf(f, "type = duration\n");
    print_metric_meta(f, item->meta);
    switch (config->duration_aggregation_type) {
        case DURATION_AGGREGATION_TYPE_BASIC:
            print_exact_durations(f, (exact_duration_collection*)item->value);
            break;
        case DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM:
            print_hdr_durations(f, (struct hdr_histogram*)item->value);
    }
    fprintf(f, "\n");
}

/**
 * Frees duration metric value
 * @arg config
 * @arg metric - Metric value to be freed
 */
void free_duration_value(agent_config* config, metric* item) {
    switch (config->duration_aggregation_type) {
        case DURATION_AGGREGATION_TYPE_BASIC:
            free_exact_duration_value(config, item);
            break;
        case DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM:
            free_hdr_duration_value(config, item);
            break;
    }
}
