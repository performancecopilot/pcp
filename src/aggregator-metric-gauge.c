#include <float.h>
#include <string.h>

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metric-gauge.h"
#include "errno.h"
#include "utils.h"

/**
 * Creates gauge metric record
 * @arg config - Config from in which gauge type is specified
 * @arg datagram - Datagram with source data
 * @arg out - Placeholder metric
 * @return 1 on success, 0 on fail
 */
int
create_gauge_metric(struct agent_config* config, struct statsd_datagram* datagram, struct metric** out) {
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
    size_t len = strlen(datagram->name) + 1;
    (*out)->name = (char*) malloc(len);
    ALLOC_CHECK("Unable to allocate memory for copy of metric name.");
    strncpy((*out)->name, datagram->name, len);
    (*out)->type = METRIC_TYPE_GAUGE;
    (*out)->value = (double*) malloc(sizeof(double));
    ALLOC_CHECK("Unable to allocate memory for copy of metric name.");
    *(double*)(*out)->value = new_value;
    (*out)->meta = create_metric_meta(datagram);
    return 1;
}

/**
 * Updates gauge metric record of value subtype
 * @arg config - Config from which we know what gauge
 * @arg item - Item to be updated
 * @arg datagram - Data to update the item with
 * @return 1 on success, 0 on fail
 */
int
update_gauge_metric(struct agent_config* config, struct metric* item, struct statsd_datagram* datagram) {
    (void)config;
    double old = *((double*) item->value);
    double new_value;
    switch (datagram->explicit_sign) {
        case SIGN_MINUS:
            new_value = -1.0 * datagram->value;
            break;
        default:
            new_value = datagram->value;
    }
    if ((old + new_value <= DBL_MAX) || (old - new_value >= -DBL_MAX)) {
        if (datagram->explicit_sign == SIGN_NONE) {
            *(double*)(item->value) = new_value;
        } else {
            *(double*)(item->value) += new_value;
        }
    }
    return 1;
}


/**
 * Prints gauge metric information
 * @arg config - Config where gauge subtype is specified
 * @arg f - Opened file handle
 * @arg item - Metric to print out
 */
void
print_gauge_metric(struct agent_config* config, FILE* f, struct metric* item) {
    (void)config;
    fprintf(f, "-----------------\n");
    fprintf(f, "name = %s\n", item->name);
    fprintf(f, "type = gauge\n");
    fprintf(f, "value = %f\n", *(double*)(item->value));
    print_metric_meta(f, item->meta);
    fprintf(f, "\n");
}

/**
 * Frees gauge metric value
 * @arg config
 * @arg metric - Metric value to be freed
 */
void
free_gauge_value(struct agent_config* config, struct metric* item) {
    (void)config;
    if (item->value != NULL) {
        free(item->value);
    }
}
