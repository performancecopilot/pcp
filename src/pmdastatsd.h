#ifndef STATSD_
#define STATSD_

#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#include "config-reader.h"
#include "aggregator-metrics.h"

#define DURATION_INDOM 0
#define METRIC_COUNTERS_INDOM 1

struct pmda_metric_helper {
    struct pmda_data_extension* data;
    const char* key;
    struct metric* item;
} pmda_metric_helper;

struct pmda_data_extension {
    struct agent_config* config;
    struct pmda_metrics_container* metrics_storage;
    struct pmda_stats_container* stats_storage;
    pmdaMetric* pcp_metrics;
    pmdaIndom* pcp_instance_domains;
    pmdaNameSpace* pcp_pmns;
    size_t pcp_instance_domain_count;
    size_t pcp_metric_count;
    size_t generation;
    int next_cluster_id;
    int next_item_id;
    int notify;
} pmda_data_extension;

#endif
