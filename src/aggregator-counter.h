#ifndef AGGREGATOR_COUNTER_
#define AGGREGATOR_COUNTER_

#include <stdio.h>
#include "config-reader.h"
#include "statsd-parsers.h"
#include "aggregators.h"

/**
 * Creates counter metric record
 * @arg config - / (safe to null)
 * @arg datagram - Datagram with source data
 * @arg out - Placeholder metric
 * @return 1 on success, 0 on fail
 */
int create_counter_metric(agent_config* config, statsd_datagram* datagram, metric** out);

/**
 * Update counter metric record
 * @arg config - / (safe to null)
 * @arg item - Item to update
 * @arg datagram - Date to update item
 * @return 1 on success, 0 on fail
 */
int update_counter_metric(agent_config* config, metric* item, statsd_datagram* datagram);

/**
 * Prints counter metric information
 * @arg config - Config where counter subtype is specified
 * @arg f - Opened file handle
 * @arg item - Metric to print out
 */
void print_counter_metric(agent_config* config, FILE* f, metric* item);

/**
 * Frees counter metric value
 * @arg config
 * @arg metric - Metric value to be freed
 */
void free_counter_value(agent_config* config, metric* item);

#endif
