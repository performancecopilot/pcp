#include <string.h>
#include <float.h>

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metric-counter.h"
#include "errno.h"
#include "utils.h"

/**
 * Creates counter metric record
 * @arg config - / (safe to null)
 * @arg datagram - Datagram with source data
 * @arg out - Placeholder metric
 * @return 1 on success, 0 on fail
 */
int
create_counter_metric(struct agent_config* config, struct statsd_datagram* datagram, struct metric** out) {
    (void)config;
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
    size_t len = strlen(datagram->name) + 1;
    (*out)->name = (char*) malloc(len);
    ALLOC_CHECK("Unable to allocate memory for copy of metric name.");
    strncpy((*out)->name, datagram->name, len);
    (*out)->type = METRIC_TYPE_COUNTER;
    (*out)->value = (double*) malloc(sizeof(double));
    ALLOC_CHECK("Unable to allocate memory for copy of metric value.");
    memcpy((*out)->value, &new_value, sizeof(double));
    (*out)->meta = create_metric_meta(datagram);
    return 1;
}

/**
 * Update counter metric record
 * @arg config - / (safe to null)
 * @arg item - Item to update
 * @arg datagram - Date to update item
 * @return 1 on success, 0 on fail
 */
int
update_counter_metric(struct agent_config* config, struct metric* item, struct statsd_datagram* datagram) {
    (void)config;
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
    *(double*)(item->value) += new_value;
    return 1;
}

/**
 * Prints counter metric information
 * @arg config - Config where counter subtype is specified
 * @arg f - Opened file handle
 * @arg item - Metric to print out
 */
void
print_counter_metric(struct agent_config* config, FILE* f, struct metric* item) {
    (void)config;
    fprintf(f, "-----------------\n");
    fprintf(f, "name = %s\n", item->name);
    fprintf(f, "type = counter\n");
    fprintf(f, "value = %f\n", *(double*)(item->value));
    print_metric_meta(f, item->meta);
    fprintf(f, "\n");
}

/**
 * Frees counter metric value
 * @arg config
 * @arg metric - Metric value to be freed
 */
void
free_counter_value(struct agent_config* config, struct metric* item) {
    (void)config;
    if (item->value != NULL) {
        free(item->value);
    }
}
