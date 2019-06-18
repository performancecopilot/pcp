#include <float.h>
#include <string.h>

#include "config-reader.h"
#include "statsd-parsers.h"
#include "consumers.h"
#include "consumer-gauge.h"
#include "errno.h"

/**
 * Creates gauge metric record
 * @arg config - Config from in which gauge type is specified
 * @arg datagram - Datagram with source data
 * @arg out - Placeholder metric
 * @return 1 on success, 0 on fail
 */
int create_gauge_metric(agent_config* config, statsd_datagram* datagram, metric** out) {
    (void)config;
    if (datagram->metric == NULL) {
        return 0;
    }
    double value = strtod(datagram->value, NULL);
    if (errno == ERANGE) {
        return 0;
    }
    (*out)->name = malloc(strlen(datagram->metric));
    strcpy((*out)->name, datagram->metric);
    (*out)->type = (METRIC_TYPE*) malloc(sizeof(METRIC_TYPE));
    *((*out)->type) = 2;
    (*out)->value = (double*) malloc(sizeof(double));
    *((double*)(*out)->value) = value;
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
int update_gauge_metric(agent_config* config, metric* item, statsd_datagram* datagram) {
    (void)config;
    int substract = 0;
    int add = 0;
    if (datagram->value[0] == '+') {
        add = 1;
    }
    if (datagram->value[0] == '-') {
        substract = 1; 
    }
    double value;
    if (substract || add) {
        char* substr = &(datagram->value[1]);
        value = strtod(substr, NULL);
    } else {
        value = strtod(datagram->value, NULL);
    }
    if (errno == ERANGE) {
        return 0;
    }
    double old_value = *(double*)(item->value);
    if (add || substract) {
        if (add) {
            if (old_value + value >= DBL_MAX) {
                return 0;
            }
            *(double*)(item->value) += value;
        }
        if (substract) {
            if (old_value - value <= -DBL_MAX) {
                return 0;
            }
            *(double*)(item->value) -= value;
        }
    } else {
        *(double*)(item->value) = value;
    }
    return 1;
}


/**
 * Prints gauge metric information
 * @arg config - Config where gauge subtype is specified
 * @arg f - Opened file handle
 * @arg item - Metric to print out
 */
void print_gauge_metric(agent_config* config, FILE* f, metric* item) {
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
void free_gauge_value(agent_config* config, metric* item) {
    (void)config;
    if (item->value != NULL) {
        free(item->value);
    }
}
