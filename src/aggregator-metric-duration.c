#include <hdr/hdr_histogram.h>
#include <string.h>
#include <float.h>

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metric-duration.h"
#include "aggregator-metric-duration-exact.h"
#include "aggregator-metric-duration-hdr.h"
#include "errno.h"
#include "utils.h"

/**
 * Creates duration metric record of value subtype
 * @arg config - Config from in which duration type is specified
 * @arg datagram - Datagram with source data
 * @arg out - Placeholder metric
 * @return 1 on success, 0 on fail
 */
int
create_duration_metric(struct agent_config* config, struct statsd_datagram* datagram, struct metric** out) {
    double new_value;
    switch (datagram->explicit_sign) {
        case SIGN_MINUS:
            new_value = -1.0 * datagram->value;
            break;
        default:
            new_value = datagram->value;
    }
    if (new_value < 0 || new_value >= DBL_MAX) {
        return 0;
    }
    if (config->duration_aggregation_type == DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM) {
        (*out)->value = create_hdr_duration_value((unsigned long long) new_value);
    } else {
        (*out)->value = create_exact_duration_value((unsigned long long) new_value);
    }
    size_t len = strlen(datagram->name) + 1;
    (*out)->name = (char*) malloc(len);
    ALLOC_CHECK("Unable to allocate memory for copy of metric name.");
    strncpy((*out)->name, datagram->name, len);
    (*out)->type = METRIC_TYPE_DURATION;
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
int
update_duration_metric(struct agent_config* config, struct metric* item, struct statsd_datagram* datagram) {
    double new_value;
    switch (datagram->explicit_sign) {
        case SIGN_MINUS:
            new_value = -1.0 * datagram->value;
            break;
        default:
            new_value = datagram->value;
    }
    if (new_value < 0) {
        return 0;
    }
    if (config->duration_aggregation_type == DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM) {
        update_hdr_duration_value((struct hdr_histogram*)item->value, (unsigned long long) new_value);
    } else {
        update_exact_duration_value((struct exact_duration_collection*)item->value, (unsigned long long) new_value);
    }
    return 1;
}

/**
 * Extracts duration metric meta values from duration metric record
 * @arg config - Config which contains info on which duration aggregating type we are using
 * @arg item - Metric item from which to extract duration values
 * @arg out - Dest to populate with data, allocates memory
 * @return 1 on success
 */
int
get_duration_values_meta(struct agent_config* config, struct metric* item, struct duration_values_meta* out) {
    ALLOC_CHECK("Unable to allocate memory for duration values.");
    int status = 0;
    if (config->duration_aggregation_type == DURATION_AGGREGATION_TYPE_BASIC) {
        status = get_exact_duration_values_meta((struct exact_duration_collection*)item->value, &out);
    } else {
        status = get_hdr_duration_values_meta((struct hdr_histogram*)item->value, &out);
    }
    if (status != 1) {
        VERBOSE_LOG("Failed to correctly extract duration values.");
    }
    return status;
}

/**
 * Prints duration metric information
 * @arg config - Config where duration subtype is specified
 * @arg f - Opened file handle
 * @arg item - Metric to print out
 */
void
print_duration_metric(struct agent_config* config, FILE* f, struct metric* item) {
    fprintf(f, "-----------------\n");
    fprintf(f, "name = %s\n", item->name);
    fprintf(f, "type = duration\n");
    print_metric_meta(f, item->meta);
    switch (config->duration_aggregation_type) {
        case DURATION_AGGREGATION_TYPE_BASIC:
            print_exact_durations(f, (struct exact_duration_collection*)item->value);
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
void
free_duration_value(struct agent_config* config, struct metric* item) {
    switch (config->duration_aggregation_type) {
        case DURATION_AGGREGATION_TYPE_BASIC:
            free_exact_duration_value(config, item);
            break;
        case DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM:
            free_hdr_duration_value(config, item);
            break;
    }
}
