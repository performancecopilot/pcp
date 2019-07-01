#ifndef AGGREGATORS_
#define AGGREGATORS_

#include <stddef.h>
#include <pcp/dict.h>
#include <chan/chan.h>

#include "config-reader.h"
#include "parsers.h"
#include "network-listener.h"
#include "pcp.h"

typedef dict metrics;

struct metric_metadata {
    char* tags;
    char* instance;
    char* sampling;
} metric_metadata;

enum METRIC_TYPE { 
    METRIC_TYPE_COUNTER = 0b001,
    METRIC_TYPE_GAUGE = 0b010,
    METRIC_TYPE_DURATION = 0b100
} METRIC_TYPE;

struct metric {
    char* name;
    struct metric_metadata* meta;
    void* value;
    enum METRIC_TYPE type;
} metric;

struct aggregator_args
{
    struct agent_config* config;
    chan_t* parsed_datagrams;
    chan_t* pcp_request_channel;
    chan_t* pcp_response_channel;
    metrics* metrics_wrapper;
} aggregator_args;

/**
 * Initializes metrics struct to empty values
 * @arg config - Config (should there be need to pass detailed info into metrics)
 */
metrics*
init_metrics(struct agent_config* config);

/**
 * Thread startpoint - passes down given datagram to aggregator to record value it contains
 * @arg args - (aggregator_args), see ~/src/network-listener.h
 */
void*
aggregator_exec(void* args);

/**
 * Sets flag notifying that output was requested
 */
void
aggregator_request_output();

/**
 * Processes datagram struct into metric 
 * @arg config - Agent config
 * @arg m - Metrics struct acting as metrics wrapper
 * @arg datagram - Datagram to be processed
 */
void
process_datagram(struct agent_config* config, metrics* m, struct statsd_datagram* datagram);

/**
 * NOT IMPLEMENTED
 * Processes PCP PMDA request
 * @arg config - Agent config
 * @arg m - Metric struct acting as metrics wrapper
 * @arg request - PCP PMDA request to be processed
 * @arg out - Channel over which to send request response
 */
void
process_pcp_request(struct agent_config* config, metrics* m, struct pcp_request* request, chan_t* out);

/**
 * Frees metric
 * @arg config
 * @arg metric - Metric to be freed
 */
void
free_metric(struct agent_config* config, struct metric* metric);

/**
 * Writes information about recorded metrics into file
 * @arg config - Config containing information about where to output
 * @arg m - Metrics struct (what values to print)
 */
void
print_metrics(struct agent_config* config, metrics* m);

/**
 * Finds metric by name
 * @arg name - Metric name to search for
 * @arg out - Placeholder metric
 * @return 1 when any found
 */
int
find_metric_by_name(metrics* m, char* name, struct metric** out);

/**
 * Creates metric
 * @arg config - Agent config
 * @arg datagram - Datagram with data that should populate new metric
 * @arg out - Placeholder metric
 * @return 1 on success
 */
int
create_metric(struct agent_config* config, struct statsd_datagram* datagram, struct metric** out);

/**
 * Adds metric to hashtable
 * @arg counter - Metric to be saved
 */
void
add_metric(metrics* m, char* key, struct metric* item);

/**
 * Updates counter record
 * @arg config - Agent config
 * @arg counter - Metric to be updated
 * @arg datagram - Data with which to update
 * @return 1 on success
 */
int
update_metric(struct agent_config* config, struct metric* item, struct statsd_datagram* datagram);

/**
 * Checks if given metric name is available (it isn't recorded yet)
 * @arg m - Metrics struct (storage in which to check for name availability)
 * @arg name - Name to be checked
 * @return 1 on success else 0
 */
int
check_metric_name_available(metrics* m, char* name);

/**
 * Creates metric metadata
 * @arg datagram - Datagram from which to build metadata
 * @return metric metadata
 */
struct metric_metadata*
create_metric_meta(struct statsd_datagram* datagram);

/**
 * Frees metric metadata
 * @arg metadata - Metadata to be freed
 */ 
void
free_metric_metadata(struct metric_metadata* meta);

/**
 * Prints metadata 
 * @arg f - Opened file handle, doesn't close it after finishing
 * @arg meta - Metric metadata
 */
void
print_metric_meta(FILE* f, struct metric_metadata* meta);

/**
 * Creates arguments for Agregator thread
 * @arg config - Application config
 * @arg parsed_channel - Parser -> Aggregator channel
 * @arg pcp_request_channel - PCP -> Aggregator channel
 * @arg pcp_response_channel - Aggregator -> PCP channel
 * @return aggregator_args
 */
struct aggregator_args* create_aggregator_args(
    struct agent_config* config,
    chan_t* parsed_channel,
    chan_t* pcp_request_channel,
    chan_t* pcp_response_channel,
    metrics* m
);

#endif
