#ifndef STATSD_
#define STATSD_

#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#include "config-reader.h"

struct pmda_data_extension {
    struct agent_config* config;
    struct pmda_metrics_container* metrics_storage;
    struct pmda_stats_container* stats_storage;
    pmdaMetric* pcp_metrics;
    pmdaIndom* pcp_instance_domains;
    pmdaNameSpace* pcp_pmns;
    size_t pcp_instance_domain_count;
    size_t pcp_metric_count;
    int reload;
    int notify;
} pmda_data_extension;

#endif
