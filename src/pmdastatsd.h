#ifndef STATSD_
#define STATSD_

#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#include "config-reader.h"
#include "aggregator-metrics.h"

#define STATS_METRIC_COUNTERS_INDOM 0
#define STATSD_METRIC_DEFAULT_DURATION_INDOM 1
#define STATSD_METRIC_DEFAULT_INDOM 2

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
    int notify;
} pmda_data_extension;

#endif
