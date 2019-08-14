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
#ifndef AGGREGATOR_METRIC_LABELS_
#define AGGREGATOR_METRIC_LABELS_

#include "aggregators.h"
#include "aggregator-metrics.h"

/**
 * Futher processes datagram that also possesses labels
 * @arg config - Agent config
 * @arg container - Metrics struct acting as metrics wrapper
 * @arg item - Metric serving as root
 * @arg datagram - Datagram to be processed
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
int
process_labeled_datagram(
    struct agent_config* config,
    struct pmda_metrics_container* container,
    struct metric* item,
    struct statsd_datagram* datagram
);

/**
 * Find label by name
 * @arg container - Metrics container
 * @arg metric - Within which metric to search for label
 * @arg key - Label key to find
 * @arg out - Placeholder label
 * @return 1 when any found, 0 when not
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
int
find_label_by_name(
    struct pmda_metrics_container* container,
    struct metric* item,
    char* key,
    struct metric_label** out
);

/**
 * Creates metric label child
 * @arg config - Agent config
 * @arg item - Parent metric
 * @arg datagram - Child's data
 * @arg out - Placholder metric label
 * @return 1 on success, 0 on fail
 */
int
create_label(
    struct agent_config* config,
    struct metric* item,
    struct statsd_datagram* datagram,
    struct metric_label** out
);

/**
 * Adds label to metric's children hashtable
 * @arg container - Metrics container 
 * @arg item - Metric containing children hashtable target
 * @arg key - Label key
 * @arg label - Label to be saved
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
void
add_label(struct pmda_metrics_container* container, struct metric* item, char* key, struct metric_label* label);

/**
 * Prints metric label information
 * @arg config - Config where duration subtype is specified
 * @arg f - Opened file handle
 * @arg item - Labels to print out
 */
void
print_labels(struct agent_config* config, FILE* f, labels* l);

/**
 * Frees metric label value
 * @arg config - Contains info about aggregation used
 * @arg metadata - Metadata to be freed
 */
void
free_metric_label(struct agent_config* config, struct metric_label* label);

/**
 * Frees metric label metadata
 * @arg metadata - Metadata to be freed
 */
void
free_metric_label_metadata(struct metric_label_metadata* meta);

#endif
