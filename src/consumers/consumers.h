#ifndef CONSUMERS_
#define CONSUMERS_

#include "../utils/dict.h"

typedef struct metric_metadata {
    tag_collection* tags;
    char* instance;
    char* sampling;
} metric_metadata;

typedef enum { COUNTER = 0b001, GAUGE = 0b010, DURATION = 0b100 } METRIC_TYPE;

typedef struct metric {
    char* name;
    METRIC_TYPE* type;
    metric_metadata* meta;
    void* value;
} metric;

typedef dict metrics;

/**
 * Initializes metrics struct to empty values
 * @arg config - Config (should there be need to pass detailed info into metrics)
 */
metrics* init_metrics(agent_config* config);

/**
 * Thread startpoint - passes down given datagram to consumer to record value it contains
 * @arg args - (consumer_args), see ~/statsd-parsers/statsd-parsers.h
 */
void* consume_datagram(void* args);

/**
 * Sets flag notifying that output was requested
 */
void consumer_request_output();

/**
 * Processes datagram struct into metric 
 * @arg m - Metrics struct acting as metrics wrapper
 * @arg datagram - Datagram to be processed
 */
void process_datagram(metrics* m, statsd_datagram* datagram);

/**
 * Frees metric
 * @arg metric - Metric to be freed
 */
void free_metric(metric* metric);

/**
 * Writes information about recorded metrics into file
 * @arg m - Metrics struct (what values to print)
 * @arg config - Config containing information about where to output
 */
void print_metrics(metrics* m, agent_config* config);

/**
 * Finds metric by name
 * @arg name - Metric name to search for
 * @arg out - Placeholder metric
 * @return 1 when any found
 */
int find_metric_by_name(metrics* m, char* name, metric** out);

/**
 * Creates metric
 * @arg datagram - Datagram with data that should populate new metric
 * @arg out - Placeholder metric
 * @return 1 on success
 */
int create_metric(statsd_datagram* datagram, metric** out);

/**
 * Adds metric to hashtable
 * @arg counter - Metric to be saved
 */
void save_metric(metrics* m, char* key, metric* item);

/**
 * Updates counter record
 * @arg counter - Metric to be updated
 * @arg datagram - Data with which to update
 * @return 1 on success
 */
int update_metric(metric* item, statsd_datagram* datagram);

/**
 * Checks if given metric name is available (it isn't recorded yet)
 * @arg m - Metrics struct (storage in which to check for name availability)
 * @arg name - Name to be checked
 * @return 1 on success else 0
 */
int check_metric_name_available(metrics* m, char* name);

/**
 * Creates metric metadata
 * @arg datagram - Datagram from which to build metadata
 * @return metric metadata
 */
metric_metadata* create_metric_meta(statsd_datagram* datagram);

/**
 * Frees metric metadata
 * @arg metadata - Metadata to be freed
 */ 
void free_metric_metadata(metric_metadata* metadata);


void copy_metric_meta(metric_metadata** dest, metric_metadata* src);

#endif
