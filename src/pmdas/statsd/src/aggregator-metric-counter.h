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
#ifndef AGGREGATOR_COUNTER_
#define AGGREGATOR_COUNTER_

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metrics.h"

/**
 * Creates counter value in given dest
 * @arg
 */
int
create_counter_value(struct agent_config* config, struct statsd_datagram* datagram, void** out);

/**
 * Update counter metric record
 * @arg config - / (safe to null)
 * @arg Value - Value to update
 * @arg datagram - Data to update item
 * @return 1 on success, 0 on fail
 */
int
update_counter_value(struct agent_config* config, struct statsd_datagram* datagram, void* value);

/**
 * Print counter metric value
 * @arg config
 * @arg f - Opened file handle
 * @arg value
 */
void
print_counter_metric_value(struct agent_config* config, FILE* f, void* value);

/**
 * Prints counter metric information
 * @arg config - Config where counter subtype is specified
 * @arg f - Opened file handle
 * @arg item - Metric to print out
 */
void
print_counter_metric(struct agent_config* config, FILE* f, struct metric* item);

/**
 * Frees counter metric value
 * @arg config
 * @arg value - Metric value to be freed
 */
void
free_counter_value(struct agent_config* config, void* value);

#endif
