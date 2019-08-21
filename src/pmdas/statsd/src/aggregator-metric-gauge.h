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
#ifndef AGGREGATOR_GAUGE_
#define AGGREGATOR_GAUGE_

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metrics.h"

/**
 * Creates gauge value in given dest
 * @arg config - Config from which we know what gauge
 * @arg datagram - Data to extract value from
 * @arg out - Dest pointer
 * @return 1 on success, 0 on fail
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
 * @arg value - value to be freed
 */
void
free_gauge_value(struct agent_config* config, void* value);

#endif
