#ifndef PMDA_PCP_INIT_
#define PMDA_PCP_INIT_

#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#include "config-reader.h"

struct pmda_data_extension {
    struct agent_config* config;
    char** argv;
    char* username;
    struct pmda_metrics_container* metrics_storage;
    struct pmda_stats_container* stats_storage;
    pmdaMetric* pcp_metrics;
    pmdaIndom* pcp_instance_domains;
    pmdaNameSpace* pcp_pmns;
    size_t hardcoded_metrics_count;
    size_t total_metric_count;
    int argc;
    char helpfile_path[MAXPATHLEN];
} pmda_data_extension;

pmdaInterface*
init_pmda(
    struct agent_config* config,
    struct pmda_metrics_container* metrics,
    struct pmda_stats_container* stats,
    int argc,
    char** argv
);

#endif
