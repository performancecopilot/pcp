/*
 * Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#ifndef PMDASTATSD_
#define PMDASTATSD_

#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <pcp/dict.h>

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
    dict* instance_map;
    size_t pcp_instance_domain_count;
    size_t pcp_metric_count;
    size_t pcp_hardcoded_metric_count;
    size_t pcp_hardcoded_instance_domain_count;
    size_t generation;
    int notify;
} pmda_data_extension;

#endif
