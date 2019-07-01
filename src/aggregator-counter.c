#include <string.h>

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-counter.h"
#include "errno.h"

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
    if (datagram->value[0] == '-' || datagram->value[0] == '+') {
        return 0;
    }
    long long unsigned int value = strtoull(datagram->value, NULL, 10);
    if (errno == ERANGE) {
        return 0;
    }
    (*out)->name = malloc(strlen(datagram->metric));
    strcpy((*out)->name, datagram->metric);
    (*out)->type = METRIC_TYPE_COUNTER;
    (*out)->value = (unsigned long long int*) malloc(sizeof(unsigned long long int));
    *((long long unsigned int*)(*out)->value) = value;
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
    if (datagram->value[0] == '-' || datagram->value[0] == '+') {
        return 0;
    }
    long long unsigned int value = strtoull(datagram->value, NULL, 10);
    if (errno == ERANGE) {
        return 0;
    }
    *(long long unsigned int*)(item->value) += value;
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
    fprintf(f, "value = %lld\n", *(long long unsigned int*)(item->value));
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
