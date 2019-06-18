#ifndef CONSUMERS_
#define CONSUMERS_

#include <pcp/dict.h>

typedef struct metric_metadata {
    char* tags;
    char* instance;
    char* sampling;
} metric_metadata;

typedef enum METRIC_TYPE { 
    COUNTER = 0b001,
    GAUGE = 0b010,
    DURATION = 0b100
} METRIC_TYPE;

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
 * @arg config - Agent config
 * @arg m - Metrics struct acting as metrics wrapper
 * @arg datagram - Datagram to be processed
 */
void process_datagram(agent_config* config, metrics* m, statsd_datagram* datagram);

/**
 * Frees metric
 * @arg metric - Metric to be freed
 */
void free_metric(metric* metric);

/**
 * Writes information about recorded metrics into file
 * @arg config - Config containing information about where to output
 * @arg m - Metrics struct (what values to print)
 */
void print_metrics(agent_config* config, metrics* m);

/**
 * Finds metric by name
 * @arg name - Metric name to search for
 * @arg out - Placeholder metric
 * @return 1 when any found
 */
int find_metric_by_name(metrics* m, char* name, metric** out);

/**
 * Creates metric
 * @arg config - Agent config
 * @arg datagram - Datagram with data that should populate new metric
 * @arg out - Placeholder metric
 * @return 1 on success
 */
int create_metric(agent_config* config, statsd_datagram* datagram, metric** out);

/**
 * Adds metric to hashtable
 * @arg counter - Metric to be saved
 */
void add_metric(metrics* m, char* key, metric* item);

/**
 * Updates counter record
 * @arg config - Agent config
 * @arg counter - Metric to be updated
 * @arg datagram - Data with which to update
 * @return 1 on success
 */
int update_metric(agent_config* config, metric* item, statsd_datagram* datagram);

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
void free_metric_metadata(metric_metadata* meta);

/**
 * Represents basic duration aggregation unit
 */
typedef struct bduration_collection {
    double** values;
    long int length;
} bduration_collection;

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

#endif
