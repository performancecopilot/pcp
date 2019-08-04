#include <string.h>
#include <float.h>

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metric-labels.h"
#include "aggregator-metric-counter.h"
#include "errno.h"
#include "utils.h"

/**
 * Creates counter value in given dest
 */
int
create_counter_value(struct agent_config* config, struct statsd_datagram* datagram, void** out) {
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
    *out = (double*) malloc(sizeof(double));
    ALLOC_CHECK("Unable to allocate memory for copy of metric value.");
    *(double*)*out = new_value;
    return 1;
}

/**
 * Update counter metric record
 * @arg config - / (safe to null)
 * @arg Value - Value to update
 * @arg datagram - Data to update item
 * @return 1 on success, 0 on fail
 */
int
update_counter_value(struct agent_config* config, struct statsd_datagram* datagram, void* value) {
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
    *(double*)(value) += new_value;
    return 1;
}

/**
 * Print counter metric value
 * @arg config
 * @arg f - Opened file handle
 * @arg value
 */
void
print_counter_metric_value(struct agent_config* config, FILE* f, void* value) {
    (void)config;
    fprintf(f, "value = %f\n", *(double*)(value));
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
    print_counter_metric_value(config, f, item->value);
    print_metric_meta(f, item->meta);
    print_labels(config, f, item->children);
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
