#ifndef AGGREGATOR_GAUGE_
#define AGGREGATOR_GAUGE_

#include <stdio.h>

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"

/**
 * Creates gauge metric record
 * @arg config - Config from in which gauge type is specified
 * @arg datagram - Datagram with source data
 * @arg out - Placeholder metric
 * @return 1 on success, 0 on fail
 */
int create_gauge_metric(agent_config* config, statsd_datagram* datagram, metric** out);

/**
 * Updates gauge metric record of value subtype
 * @arg config - Config from which we know what gauge
 * @arg item - Item to be updated
 * @arg datagram - Data to update the item with
 * @return 1 on success, 0 on fail
 */
int update_gauge_metric(agent_config* config, metric* item, statsd_datagram* datagram);

/**
 * Prints gauge metric information
 * @arg config - Config where gauge subtype is specified
 * @arg f - Opened file handle
 * @arg item - Metric to print out
 */
void print_gauge_metric(agent_config* config, FILE* f, metric* item);

/**
 * Frees gauge metric value
 * @arg config
 * @arg metric - Metric value to be freed
 */
void free_gauge_value(agent_config* config, metric* item);

#endif
