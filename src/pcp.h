#ifndef PCP_
#define PCP_

#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#include "config-reader.h"

struct pcp_args {
    struct agent_config* config;
    struct pmda_metrics_container* metrics_container;
    struct pmda_stats_container* stats_container;
    int argc;
    char** argv;
} pcp_args;

struct pmda_data_extension {
    struct agent_config* config;
    char** argv;
    char* username;
    struct pmda_metrics_container* metrics_container;
    struct pmda_stats_container* stats_container;
    pmdaMetric* pcp_metrics;
    pmdaIndom* pcp_instance_domains;
    pmdaNameSpace* pcp_pmns;
    pmLongOptions longopts;
    pmdaOptions opts;
    size_t hardcoded_metrics_count;
    size_t total_metric_count;
    int argc;
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
    struct pmda_metrics_container* metrics_container,
    struct pmda_stats_container* stats_container,
    int argc,
    char** argv
);

#endif
