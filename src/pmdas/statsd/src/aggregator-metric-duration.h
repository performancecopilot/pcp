/*
 * Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#ifndef AGGREGATOR_DURATION_
#define AGGREGATOR_DURATION_

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metrics.h"
#include "aggregator-metric-duration-exact.h"
#include "aggregator-metric-duration-hdr.h"

/**
 * Creates duration value in given dest
 * @arg
 */
int 
create_duration_value(struct agent_config* config, struct statsd_datagram* datagram, void** out);

/**
 * Updates duration metric record of value subtype
 * @arg config - Config from which we know what duration type is, either HDR or exact
 * @arg item - Item to be updated
 * @arg datagram - Data to update the item with
 * @return 1 on success, 0 on fail
 */
int
update_duration_value(struct agent_config* config, struct statsd_datagram* datagram, void* value);

/**
 * Extracts duration metric meta values from duration metric record
 * @arg config - Config which contains info on which duration aggregating type we are using
 * @arg value - Either "struct exact_duration_collection*" or "struct hdr_histogram*", basically value from metric that has type of "duration"
 * @arg instance - What information to extract
 * @return duration instance value
 */
double
get_duration_instance(struct agent_config* config, void* value, enum DURATION_INSTANCE instance);

/**
 * Print duration metric value
 * @arg config - Config where duration subtype is specified
 * @arg f - Opened file handle
 * @arg value
 */
void
print_duration_metric_value(struct agent_config* config, FILE* f, void* value);

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
 * @arg value - value to be freed
 */
void
free_duration_value(struct agent_config* config, void* value);

#endif
