#ifndef AGGREGATOR_GAUGE_
#define AGGREGATOR_GAUGE_

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metrics.h"

/**
 * Creates gauge value in given dest
 * @arg
 */
int
create_gauge_value(struct agent_config* config, struct statsd_datagram* datagram, void** out);

/**
 * Updates gauge metric record of value subtype
 * @arg config - Config from which we know what gauge
 * @arg item - Item to be updated
 * @arg datagram - Data to update the item with
 * @return 1 on success, 0 on fail
 */
int
update_gauge_value(struct agent_config* config, struct statsd_datagram* datagram, void* value);

/**
 * Print gauge metric value
 * @arg config
 * @arg f - Opened file handle
 * @arg value
 */
void
print_gauge_metric_value(struct agent_config* config, FILE* f, void* value);

/**
 * Prints gauge metric information
 * @arg config - Config where gauge subtype is specified
 * @arg f - Opened file handle
 * @arg item - Metric to print out
 */
void
print_gauge_metric(struct agent_config* config, FILE* f, struct metric* item);

/**
 * Frees gauge metric value
 * @arg config
 * @arg value - value value to be freed
 */
void
free_gauge_value(struct agent_config* config, void* value);

#endif
