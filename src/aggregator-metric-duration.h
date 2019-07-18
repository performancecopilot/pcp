#ifndef AGGREGATOR_DURATION_
#define AGGREGATOR_DURATION_

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metrics.h"
#include "aggregator-metric-duration-exact.h"
#include "aggregator-metric-duration-hdr.h"

/**
 * Creates duration metric record of value subtype
 * @arg config - Config from in which duration type is specified
 * @arg datagram - Datagram with source data
 * @arg out - Placeholder metric
 * @return 1 on success, 0 on fail
 */
int
create_duration_metric(struct agent_config* config, struct statsd_datagram* datagram, struct metric** out);

/**
 * Updates duration metric record of value subtype
 * @arg config - Config from which we know what duration type is, either HDR or exact
 * @arg item - Item to be updated
 * @arg datagram - Data to update the item with
 * @return 1 on success, 0 on fail
 */
int
update_duration_metric(struct agent_config* config, struct metric* item, struct statsd_datagram* datagram);

/**
 * Prints duration metric information
 * @arg config - Config where duration subtype is specified
 * @arg f - Opened file handle
 * @arg item - Metric to print out
 */
void
print_duration_metric(struct agent_config* config, FILE* f, struct metric* item);

/**
 * Frees duration metric value
 * @arg config
 * @arg metric - Metric value to be freed
 */
void
free_duration_value(struct agent_config* config, struct metric* item);

#endif
