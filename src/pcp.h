#ifndef PCP_
#define PCP_

#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#include "config-reader.h"

struct pcp_request {
    // not sure what to put here yet
} pcp_request;

struct pcp_args {
    struct agent_config* config;
    chan_t* aggregator_request_channel;
    chan_t* aggregator_response_channel;
    chan_t* stats_sink;
    int argc;
    char** argv;
} pcp_args;

struct pmda_stats {
    unsigned long int received;
    unsigned long int parsed;
    unsigned long int thrown_away;
    unsigned long int aggregated;
    unsigned long int time_spent_parsing;
    unsigned long int time_spent_aggregating;
    pthread_mutex_t received_lock;
    pthread_mutex_t parsed_lock;
    pthread_mutex_t thrown_away_lock;
    pthread_mutex_t aggregated_lock;
    pthread_mutex_t time_spent_parsing_lock;
    pthread_mutex_t time_spent_aggregating_lock;
} pmda_stats;

struct pmda_data_extension {
    struct agent_config* config;
    chan_t* pcp_to_aggregator;
    chan_t* aggregator_to_pcp;
    chan_t* stats_sink;
    char** argv;
    char* username;
    struct pmda_stats* stats;
    size_t hardcoded_metrics_count;
    size_t total_metric_count;
    pmdaMetric* metrics;
    pmdaIndom* instance_domains;
    pmdaNameSpace* pmns;
    int argc;
    pmdaOptions opts;
    pmLongOptions longopts;
    char helpfile_path[MAXPATHLEN];
} pmda_data_extension;

/**
 * Main loop handling incoming responses from aggregators
 * @arg args - Arguments passed to the thread
 */
void*
pcp_pmda_exec(void* args);

/**
 * Creates arguments for PCP thread
 * @arg config - Application config
 * @arg aggregator_request_channel - Aggregator -> PCP channel
 * @arg aggregator_response_channel - PCP -> Aggregator channel
 * @return pcp_args
 */
struct pcp_args*
create_pcp_args(
    struct agent_config* config,
    chan_t* aggregator_request_channel,
    chan_t* aggregator_response_channel,
    chan_t* stats_sink,
    int argc,
    char** argv
);

#endif
