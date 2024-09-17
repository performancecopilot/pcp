/*
 * Copyright (c) 2020 Red Hat.
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
#ifndef AGGREGATOR_METRICS_
#define AGGREGATOR_METRICS_

#include <stddef.h>
#include <pcp/dict.h>
#include <chan/chan.h>

#include "config-reader.h"
#include "parsers.h"
#include "network-listener.h"

typedef dict metrics;
typedef dict labels;

typedef enum DURATION_INSTANCE {
    DURATION_MIN,
    DURATION_MAX,
    DURATION_MEDIAN,
    DURATION_AVERAGE,
    DURATION_PERCENTILE90,
    DURATION_PERCENTILE95,
    DURATION_PERCENTILE99,
    DURATION_COUNT,
    DURATION_STANDARD_DEVIATION
} DURATION_INSTANCE;

typedef struct pmdaInstid_map {
    char** labels;
    size_t length;
} pmdaInstid_map;

typedef struct metric_metadata {
    char* pcp_name; // name within pcp pmns
    struct pmdaInstid_map* pcp_instance_map; 
    double sampling; // not used for anything as of right now
    pmID pmid; // this could be saved as char* as we convert it most of the time we access it
    pmInDom pmindom;
    size_t pcp_instance_domain_index;
    size_t pcp_metric_index;
    int pcp_instance_change_requested; // flag signaling to pcp that change to instance is requested
} metric_metadata;

typedef struct metric_label_metadata {
    char* instance_label_segment_str;
} metric_label_metadata;

typedef struct metric_label {
    char* labels;
    int pair_count;
    struct metric_label_metadata* meta;
    enum METRIC_TYPE type; // either this or parent reference, so that we know how to free void* value
    void* value;
} metric_label;

typedef struct metric {
    char* name;
    int committed;
    struct metric_metadata* meta;
    labels* children;
    enum METRIC_TYPE type;
    void* value;
} metric;

/**
 * Collection of metadata of some duration collection 
 */
typedef struct duration_values_meta {
    double min;
    double max;
    double median;
    double average;
    double percentile90;
    double percentile95;
    double percentile99;
    double count;
    double std_deviation;
} duration_values_meta;

typedef struct pmda_metrics_container {
    metrics* metrics;
    struct pmda_metrics_dict_privdata* metrics_privdata;
    size_t generation;
    pthread_mutex_t mutex;
} pmda_metrics_container;

typedef struct pmda_metrics_dict_privdata {
    struct agent_config* config;
    struct pmda_metrics_container* container;
} pmda_metrics_dict_privdata;

/**
 * Creates new pmda_metrics_container structure, initializes all stats to 0
 */
extern struct pmda_metrics_container*
init_pmda_metrics(struct agent_config* config);

/**
 * Creates STATSD metric hashtable key for use in hashtable related functions (find_metric_by_name, check_metric_name_available)
 * @return new key
 */
extern char*
create_metric_dict_key(char* key);

/**
 * Processes datagram struct into metric 
 * @arg config - Agent config
 * @arg container - Metrics struct acting as metrics wrapper
 * @arg datagram - Datagram to be processed
 * @return status - 1 when successfully saved or updated, 0 when thrown away
 */
int
process_metric(struct agent_config* config, struct pmda_metrics_container* container, struct statsd_datagram* datagram);

/**
 * Frees metric
 * @arg config - Agent config
 * @arg metric - Metric to be freed
 */
extern void
free_metric(struct agent_config* config, struct metric* item);

/**
 * Writes information about recorded metrics into file
 * @arg config - Config containing information about where to output
 * @arg container - Metrics struct acting as metrics wrapper
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
extern void
write_metrics_to_file(struct agent_config* config, struct pmda_metrics_container* container);

/**
 * Finds metric by name
 * @arg container - Metrics container
 * @arg key - Metric key to find
 * @arg out - Placeholder metric
 * @return 1 when any found, 0 when not
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
extern int
find_metric_by_name(struct pmda_metrics_container* container, char* key, struct metric** out);

/**
 * Creates metric
 * @arg config - Agent config
 * @arg datagram - Datagram with data that should populate new metric
 * @arg out - Placeholder metric
 * @return 1 on success
 */
extern int
create_metric(struct agent_config* config, struct statsd_datagram* datagram, struct metric** out);

/**
 * Adds metric to hashtable
 * @arg container - Metrics container 
 * @arg item - Metric to be saved
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
extern void
add_metric(struct pmda_metrics_container* container, char* key, struct metric* item);

/**
 * Removes metric from hashtable
 * @arg container - Metrics container
 * @arg key - Metric's hashtable key
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
extern void
remove_metric(struct pmda_metrics_container* container, char* key);

/**
 * Updates metric record
 * @arg config - Agent config
 * @arg container - Metrics container
 * @arg type - What type the metric value is
 * @arg datagram - Data with which to update
 * @arg value - Dest value
 * @return 1 on success, 0 when update itself fails, -1 when metric with same name but different type is already recorded
 * 
 * Synchronized by mutex on pmda_metrics_container
 */
extern int
update_metric_value(
    struct agent_config* config,
    struct pmda_metrics_container* container,
    enum METRIC_TYPE type,
    struct statsd_datagram* datagram,
    void** value
);

/**
 * Checks if given metric name is available (it isn't recorded yet or is blocklisted)
 * @arg container - Metrics container
 * @arg key - Key of metric
 * @return 1 on success else 0
 */
extern int
check_metric_name_available(struct pmda_metrics_container* container, char* key);

/**
 * Creates metric metadata
 * @arg datagram - Datagram from which to build metadata
 * @return metric metadata
 */
extern struct metric_metadata*
create_metric_meta(struct statsd_datagram* datagram);

/**
 * Frees metric metadata
 * Doesn't free individual pointers of meta->pcp_instance_map->labels as those are also pointed at
 * by metric_label* labels field
 * @arg meta - Metadata to be freed
 */
extern void
free_metric_metadata(struct metric_metadata* meta);

/**
 * Prints metadata 
 * @arg f - Opened file handle, doesn't close it after finishing
 * @arg meta - Metric metadata
 */
extern void
print_metric_meta(FILE* f, struct metric_metadata* meta);

/**
 * Special case handling - this confirms that label was also added to metric before it actually is processed
 * @arg container - Metrics container
 * @arg item - Metric to be updated
 * 
 * Synchronized by mutex on pmda_metrics_container struct
 */
extern void
mark_metric_as_committed(struct pmda_metrics_container* container, struct metric* item);

#endif
