#ifndef STATSD_
#define STATSD_

#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <pcp/dict.h>

#include "config-reader.h"

#define DURATION_INDOM 0

struct pcp_reverse_lookup_record {
    char* name;
} pcp_reverse_lookup_record;

struct pmda_data_extension {
    struct agent_config* config;
    struct pmda_metrics_container* metrics_storage;
    struct pmda_stats_container* stats_storage;
    pmdaMetric* pcp_metrics;
    pmdaIndom* pcp_instance_domains;
    pmdaNameSpace* pcp_pmns;
    dict* pcp_metric_reverse_lookup;
    size_t pcp_instance_domain_count;
    size_t pcp_metric_count;
    size_t generation;
    int next_cluster_id;
    int next_item_id;
    int notify;
} pmda_data_extension;

#endif
