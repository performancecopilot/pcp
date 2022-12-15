/*
 * Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
 * Copyright (c) 2022 Red Hat.
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
#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <hdr/hdr_histogram.h>

#include "pmdastatsd.h"
#include "parsers-utils.h"
#include "aggregator-metrics.h"
#include "aggregator-stats.h"
#include "aggregator-metric-labels.h"
#include "aggregator-metric-duration-exact.h"
#include "utils.h"
#include "config-reader.h"
#include "pmda-callbacks.h"
#include "domain.h"

/**
 * Gets next valid pmID
 * - used for getting pmIDs for yet unrecorded metrics
 * - we start from cluster #1, because #0 is reserved for agent stats
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 * @return new valid pmID
 */
static pmID
get_next_pmID(pmdaExt* pmda) {
    static int next_cluster_id = 1;
    static int next_item_id = 0;
    pmID next = pmID_build(pmda->e_domain, next_cluster_id, next_item_id);
    if (next_cluster_id >= (1 << 12)) {
        DIE("Agent ran out of metric ids.");
    }
    if (next_item_id == 1000) {
        next_item_id = 0;
        next_cluster_id += 1;
    } else {
        next_item_id += 1;
    }
    return next;
}

/**
 * Gets next valid pmInDom
 * - used for getting pmInDoms for yet unrecorded metrics
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 * @return new valid pmInDom
 */
static pmInDom
get_next_pmInDom(pmdaExt* pmda) {
    static int next_pmindom = 3; 
    pmInDom next = pmInDom_build(pmda->e_domain, next_pmindom);
    if (next_pmindom - 1 == (1 << 22)) {
        DIE("Agent ran out of metric instance domains.");
    }
    next_pmindom += 1;
    return next;
}

/**
 * Creates new key for instance map, that will be use to map metric to instance (only custom instances, where this relationship is 1:1)
 * @arg indom - instance domain
 * @return key for instance_map
 */
static char*
create_instance_map_key(pmInDom indom) {
    char buffer[JSON_BUFFER_SIZE];
    int l = pmsprintf(buffer, JSON_BUFFER_SIZE, "%s", pmInDomStr(indom)) + 1;
    char* key = malloc(sizeof(char) * l);
    ALLOC_CHECK(key, "Unable to allocate memory for instance key");
    memcpy(key, buffer, l);
    return key;
}

/**
 * Adds new metric to pcp metric table and pcp pmns and then increments total metric count
 * @arg key  - Key under which metric is saved in hashtable
 * @arg item - StatsD Metric from which to extract what PCP Metric to create, assigns PMID and PCP name to metric as a side-effect
 * @arg data - PMDA extension structure (contains agent-specific private data)
 */
static void
create_pcp_metric(char* key, struct metric* item, pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData(pmda);    
    data->pcp_metrics = realloc(data->pcp_metrics, sizeof(pmdaMetric) * (data->pcp_metric_count + 1));
    ALLOC_CHECK(data->pcp_metrics, "Cannot grow statsd metric list.");
    // .. with new metric description 
    size_t i = data->pcp_metric_count;
    pmID newpmid = get_next_pmID(pmda);
    pmdaMetric* new_metric = &data->pcp_metrics[i];
    struct pmda_metric_helper* helper = (struct pmda_metric_helper*) malloc(sizeof(struct pmda_metric_helper));
    ALLOC_CHECK(helper, "Unable to allocate mem for metric helper struct.");
    helper->key = key;
    helper->item = item;
    helper->data = data;
    new_metric->m_user = helper;
    new_metric->m_desc.pmid = newpmid;
    new_metric->m_desc.type = PM_TYPE_DOUBLE;
    new_metric->m_desc.indom = item->meta->pmindom;
    if (item->type == METRIC_TYPE_COUNTER) {
        new_metric->m_desc.sem = PM_SEM_COUNTER;
    } else {
        new_metric->m_desc.sem = PM_SEM_INSTANT;
    }
    if (item->type == METRIC_TYPE_DURATION) {
        new_metric->m_desc.units.dimSpace = 0;
        new_metric->m_desc.units.dimCount = 0;
        new_metric->m_desc.units.pad = 0;
        new_metric->m_desc.units.scaleSpace = 0;
        new_metric->m_desc.units.dimTime = 1;
        new_metric->m_desc.units.scaleTime = PM_TIME_MSEC;
        new_metric->m_desc.units.scaleCount = 1;
    } else {
        memset(&new_metric->m_desc.units, 0, sizeof(pmUnits));
    }
    item->meta->pmid = newpmid;
    VERBOSE_LOG(
        1,
        "STATSD: adding metric %s %s from %s\n", item->meta->pcp_name, pmIDStr(item->meta->pmid), item->name
    );
    item->meta->pcp_metric_index = i;
}

/***
 * Maps metric_labels and root values of given metric to PCP metric instance domain
 * @arg item - Target metric
 * @arg data - PMDA data extension (contains pcp_metrics, pcp_instances and so on)
 * @arg indom_i - Target instance domain index in data->pcp_instances
 */
static void
map_labels_to_instances(struct metric* item, struct pmda_data_extension* data, size_t indom_i) {
    size_t indom_i_inst_cnt = 0;
    int has_root_value = item->value != NULL;
    int root_offset = 0;
    if (has_root_value) {
        root_offset = 1;
    }
    size_t labels_count = dictSize(item->children);
    if (item->type == METRIC_TYPE_DURATION) {
        // because duration has 9 instances per metric/metric_label record
        indom_i_inst_cnt = (labels_count + root_offset) * 9;
    } else {
        // because counter/gauge have 1 instance per metric/metric_label record
        indom_i_inst_cnt = labels_count + root_offset;
    }
    pmdaInstid* instances =
        (pmdaInstid*) malloc(sizeof(pmdaInstid) * indom_i_inst_cnt);
    ALLOC_CHECK(instances, "Unable to allocate memory for new PMDA instance domain instances.");
    if (has_root_value) {
        if (item->type == METRIC_TYPE_DURATION) {
            // reuse static pmdaInstid* instances from STATSD_METRIC_DEFAULT_DURATION_INDOM domain
            pmdaInstid* static_instances = data->pcp_instance_domains[STATSD_METRIC_DEFAULT_DURATION_INDOM].it_set;
            size_t hardcoded_inst_desc_cnt = 9;
            size_t i;
            for (i = 0; i < hardcoded_inst_desc_cnt; i++) {
                instances[i] = static_instances[i];
            }
        } else {
            // reuse static pmdaInstid* instances from STATSD_METRIC_DEFAULT_INDOM domain
            instances[0] = data->pcp_instance_domains[STATSD_METRIC_DEFAULT_INDOM].it_set[0];
        }
    }
    // - prepare map
    item->meta->pcp_instance_map =
        (struct pmdaInstid_map*) malloc(sizeof(pmdaInstid_map));
    ALLOC_CHECK(item->meta->pcp_instance_map, "Unable to allocate memory for new instance domain map.");
    item->meta->pcp_instance_map->length = labels_count;
    item->meta->pcp_instance_map->labels = (char**) malloc(sizeof(char*) * labels_count);
    ALLOC_CHECK(item->meta->pcp_instance_map->labels, "Unable to allocate memory for new instance domain map label references.");
    // - prepare for iteration
    char buffer[JSON_BUFFER_SIZE];
    size_t instance_name_length;
    //   - prepare format strings for duration instances
    static char* duration_metric_instance_keywords[] = {
        "/min::%s",
        "/max::%s",
        "/median::%s",
        "/average::%s",
        "/percentile90::%s",
        "/percentile95::%s",
        "/percentile99::%s",
        "/count::%s",
        "/std_deviation::%s"
    };
    // iterate over metric labels and fill remaining instances, recorded order in map on metric struct which will contain references to equivalent metric_label hashtable keys
    size_t label_index = 0;
    static size_t keywords_count = 9;
    // - iterate
    dictIterator* iterator = dictGetSafeIterator(item->children);
    dictEntry* current;    
    while ((current = dictNext(iterator)) != NULL) {
        struct metric_label* label = (struct metric_label*)current->v.val;
        // store on which instance domain instance index we find current label,
        item->meta->pcp_instance_map->labels[label_index] = label->labels;
        if (label->type == METRIC_TYPE_DURATION) {
            // duration metric has 9 instances per single metric_label, formatted with premade label string segments
            size_t i = 0;
            int label_offset = (label_index + root_offset) * 9;
            for (i = 0; i < keywords_count; i++) {
                instances[label_offset + i].i_inst = label_offset + i;
                instance_name_length = 
                    pmsprintf(
                        buffer,
                        JSON_BUFFER_SIZE,
                        duration_metric_instance_keywords[i],
                        label->meta->instance_label_segment_str
                    ) + 1;
                instances[label_offset + i].i_name = (char*) malloc(sizeof(char) * instance_name_length);
                ALLOC_CHECK(instances[label_offset + i].i_name, "Unable to allocate memory for instance description.");
                memcpy(instances[label_offset + i].i_name, buffer, instance_name_length);
            }
        } else {
            int label_offset = (label_index + root_offset);
            // counter/gauge has 1 instance per single metric_label, just add / to premade label string
            instances[label_offset].i_inst = label_offset;
            instance_name_length = pmsprintf(buffer, JSON_BUFFER_SIZE, "/%s", label->meta->instance_label_segment_str) + 1;
            instances[label_offset].i_name = (char*) malloc(sizeof(char) * instance_name_length);
            ALLOC_CHECK(instances[label_offset].i_name, "Unable to allocate memory for instance description.");
            memcpy(instances[label_offset].i_name, buffer, instance_name_length);
        }
        label_index++;
    }
    dictReleaseIterator(iterator);
    data->pcp_instance_domains[indom_i].it_numinst = indom_i_inst_cnt;
    data->pcp_instance_domains[indom_i].it_set = instances;
    VERBOSE_LOG(
        1,
        "STATSD: mapped labels to instances for metric %s %s from %s", item->meta->pcp_name, pmIDStr(item->meta->pmid), item->name
    );
}

/***
 * Creates instance domain for given metric
 * @arg key - Metric key
 * @arg item - Target metric
 * @arg pmda - pdmaExt*
 */
static void
create_instance(char* key, struct metric* item, pmdaExt* pmda) {
    (void)key;
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData((pmdaExt*)pmda);
    // get new pmInDom
    pmInDom next_pmindom = get_next_pmInDom(pmda);
    // add new pmdaIndom to domain list
    data->pcp_instance_domains = 
        (pmdaIndom*) realloc(
            data->pcp_instance_domains,
            (data->pcp_instance_domain_count + 1) * sizeof(pmdaIndom)
        );
    ALLOC_CHECK(data->pcp_instance_domains, "Unable to resize memory for PMDA instance domains");
    char* instance_key = create_instance_map_key(next_pmindom);
    dictAdd(data->instance_map, instance_key, item->name);
    free(instance_key);
    size_t indom_i = data->pcp_instance_domain_count;
    // build new pmdaInstid
    map_labels_to_instances(item, data, indom_i);    
    // populate new instance domain
    data->pcp_instance_domains[indom_i].it_indom = next_pmindom;
    // current metric instance domain has changed
    item->meta->pmindom = next_pmindom;
    // save on which index instance domain for current metric is stored
    item->meta->pcp_instance_domain_index = indom_i;
    // update instance in pcp metric_record
    data->pcp_metrics[item->meta->pcp_metric_index].m_desc.indom = next_pmindom;
    // we added new instance domain 
    data->pcp_instance_domain_count += 1;
    // (and since the metric record couldnt be updated while we did it)
    item->meta->pcp_instance_change_requested = 0;
}

/**
 * Updates instance for labeled StatsD metric
 * - frees up old instance data that is not shared
 * - remaps StatsD metric labels to instance
 * - clears flag requesting instance update
 * @arg key - Metric key
 * @arg item - Metric which instance should be updated
 * @arg pmda - 
 */
static void
update_instance(char* key, struct metric* item, pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData(pmda);
    size_t indom_i = item->meta->pcp_instance_domain_index;
    pmdaIndom domain = data->pcp_instance_domains[indom_i];
    int i;
    for (i = 0; i < domain.it_numinst; i++) {
        if (item->type == METRIC_TYPE_DURATION) {
            // skip some names as they are shared between all duration intances
            if (i > 8) {
                free(domain.it_set[i].i_name);
            }
        } else {
            // skip some names as they are shared between all counter/gauge instances
            if (i > 0) {
                free(domain.it_set[i].i_name);
            }
        }
    }
    free(domain.it_set);
    free(item->meta->pcp_instance_map->labels);
    free(item->meta->pcp_instance_map);
    map_labels_to_instances(item, data, indom_i);
    // (and since the metric record couldnt be updated while we did it)
    item->meta->pcp_instance_change_requested = 0;
}

/**
 * Updates metric's instances domain, be it either updating existing instance domain or creating new one for it
 * @arg key - key under which the metric is saved
 * @arg item - metric
 * @arg pmda - pmdaExt
 */
static void
update_pcp_metric_instance_domain(char* key, struct metric* item, pmdaExt* pmda) {
    // no reason to update if metric has no labels
    if (item->children == NULL) return;
    // these are only for comparison
    if (pmInDom_serial(item->meta->pmindom) == STATSD_METRIC_DEFAULT_DURATION_INDOM ||
        pmInDom_serial(item->meta->pmindom) == STATSD_METRIC_DEFAULT_INDOM) {
        create_instance(key, item, pmda);
    } else {
        update_instance(key, item, pmda);
    }
    VERBOSE_LOG(
        1,
        "Updated instance domain for metric %s %s from %s\n",
        item->meta->pcp_name, pmIDStr(item->meta->pmid), item->name
    );
}

/**
 * This gets called for every StatsD metric that has been aggregated already,
 * registers it within PCP space and while doing that assigns it unique PMID and PCP metric name 
 * @arg key  - Key under which metric is saved in hashtable
 * @arg item - Metric that is to be processed
 * @arg pmda - pmdaExt
 * 
 * Synchronized - Metric collection is locked while this is ongoing
 */
static void
map_metric(char* key, struct metric* item, void* pmda) {
    // skip metric if its still being processed (case when new metric gets added and datagram has only label, but metric addition and label appending are 2 separate actions)
    if (item->pernament == 0) return;
    // this prevents creating of metrics/labels that have tags as we don't deal with those yet
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData((pmdaExt*)pmda);
    // lets check if metric already has pmid assigned, if not create new metric for it (including new pmid)
    if (item->meta->pmid == PM_ID_NULL) {
        create_pcp_metric(key, item, (pmdaExt*)pmda);
    }
    // pcp_instance_change_requested flag is set, when metric gets new metric_label
    if (item->meta->pcp_instance_change_requested == 1) {
        update_pcp_metric_instance_domain(key, item, (pmdaExt*)pmda);
    }
    data->pcp_metric_count += 1;
    process_stat(data->config, data->stats_storage, STAT_TRACKED_METRIC, (void*)item->type);
    VERBOSE_LOG(1, "Populated PMNS with %d, %s .", item->meta->pmid, item->meta->pcp_name);
    pmdaTreeInsert(data->pcp_pmns, item->meta->pmid, item->meta->pcp_name);
}

static void
insert_hardcoded_metrics(pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*) pmdaExtGetData(pmda);
    char name[64];
    pmsprintf(name, 64, "statsd.pmda.received");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 0), name);
    pmsprintf(name, 64, "statsd.pmda.parsed");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 1), name);
    pmsprintf(name, 64, "statsd.pmda.dropped");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 2), name);
    pmsprintf(name, 64, "statsd.pmda.aggregated");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 3), name);
    pmsprintf(name, 64, "statsd.pmda.metrics_tracked");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 4), name);
    pmsprintf(name, 64, "statsd.pmda.time_spent_parsing");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 5), name);
    pmsprintf(name, 64, "statsd.pmda.time_spent_aggregating");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 6), name);
    pmsprintf(name, 64, "statsd.pmda.settings.max_udp_packet_size");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 7), name);
    pmsprintf(name, 64, "statsd.pmda.settings.max_unprocessed_packets");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 8), name);
    pmsprintf(name, 64, "statsd.pmda.settings.verbose");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 9), name);
    pmsprintf(name, 64, "statsd.pmda.settings.debug_output_filename");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 10), name);
    pmsprintf(name, 64, "statsd.pmda.settings.port");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 11), name);
    pmsprintf(name, 64, "statsd.pmda.settings.parser_type");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 12), name);
    pmsprintf(name, 64, "statsd.pmda.settings.duration_aggregation_type");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 13), name);
    VERBOSE_LOG(1, "Populated PMNS with hardcoded metrics.");
}

/**
 * Maps all stats (both hardcoded and the ones aggregated from StatsD datagrams) to 
 */
static void
statsd_map_stats(pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*) pmdaExtGetData(pmda);
    int status = 0;
    if (data->pcp_pmns) {
        pmdaTreeRelease(data->pcp_pmns);
        VERBOSE_LOG(1, "Released PMNS.");
        data->notify |= PMDA_EXT_NAMES_CHANGE; 
    }
    status = pmdaTreeCreate(&data->pcp_pmns);
    if (status < 0) {
        VERBOSE_LOG(1, "%s: failed to create new pmns: %s\n", pmGetProgname(), pmErrStr(status));
        data->pcp_pmns = NULL;
        return;
    } 
    reset_stat(data->config, data->stats_storage, STAT_TRACKED_METRIC);
    insert_hardcoded_metrics(pmda);
    struct pmda_metrics_container* container = data->metrics_storage;
    pthread_mutex_lock(&container->mutex);
    metrics* m = container->metrics;
    dictIterator* iterator = dictGetSafeIterator(m);
    dictEntry* current;
    while ((current = dictNext(iterator)) != NULL) {
        struct metric* item = (struct metric*)current->v.val;
        char* key = (char*)current->key;
        map_metric(key, item, pmda);
    }
    dictReleaseIterator(iterator);
    data->generation = data->metrics_storage->generation;
    pthread_mutex_unlock(&container->mutex);

    pmdaTreeRebuildHash(data->pcp_pmns, data->pcp_metric_count);
}

/**
 * Checks if we need to reload metric namespace. Possible causes:
 * - yet unmapped metric received
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
static void
statsd_possible_reload(pmdaExt* pmda) {    
    struct pmda_data_extension* data = (struct pmda_data_extension*) pmdaExtGetData(pmda);
    pthread_mutex_lock(&data->metrics_storage->mutex);
    int need_reload = data->metrics_storage->generation != data->generation ? 1 : 0;
    pthread_mutex_unlock(&data->metrics_storage->mutex);
    if (need_reload) {
        VERBOSE_LOG(1, "statsd: %s: reloading", pmGetProgname());
        statsd_map_stats(pmda);
        pmda->e_indoms = data->pcp_instance_domains;
        pmda->e_nindoms = data->pcp_instance_domain_count;
        pmdaRehash(pmda, data->pcp_metrics, data->pcp_metric_count);
        VERBOSE_LOG(
            1,
            "statsd: %s: %lu metrics and %lu instance domains after reload",
            pmGetProgname(),
            data->pcp_metric_count,
            data->pcp_instance_domain_count
        );
    }
}

/**
 * Maps duration instance to duration enum
 * - just applies % available duration stats to instance
 * @arg instance
 * @return equivalent DURATION_INSTANCE enum
 */
static enum DURATION_INSTANCE
map_to_duration_instance(int instance) {
    int i = instance % 9;
    switch (i) {
        case 0:
            return DURATION_MIN;
        case 1:
            return DURATION_MAX;
        case 2:
            return DURATION_MEDIAN;
        case 3:
            return DURATION_AVERAGE;
        case 4:
            return DURATION_PERCENTILE90;
        case 5:
            return DURATION_PERCENTILE95;
        case 6:
            return DURATION_PERCENTILE99;
        case 7:
            return DURATION_COUNT;
        case 8:
            return DURATION_STANDARD_DEVIATION;
    }
    return DURATION_COUNT;
}

/**
 * Wrapper around pmdaDesc, called before control is passed to pmdaDesc
 * @arg pm_id - Instance domain
 * @arg desc - Performance Metric Descriptor
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_desc(pmID pm_id, pmDesc* desc, pmdaExt* pmda) {
    statsd_possible_reload(pmda);
    return pmdaDesc(pm_id, desc, pmda);
}

/**
 * Wrapper around pmdaText, called before control is passed to pmdaText
 * @arg ident - Identifier
 * @arg type - Base data type
 * @arg buffer - Buffer where to write description
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_text(int ident, int type, char** buffer, pmdaExt* pmda) {
    statsd_possible_reload(pmda);
    if (pmID_cluster(ident) == 0) {
        switch (pmID_item(ident)) {
            case 0: 
            {
                static char oneliner[] = "Received datagrams count";
                static char full_description[] = 
                    "Number of datagrams/packets that the agent has received\n"
                    "during its lifetime.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 1:
            {
                static char oneliner[] = "Parsed datagrams count";
                static char full_description[] = 
                    "Number of datagrams/packets that the agent has parsed\n"
                    "successfuly during its lifetime.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 2:
            {
                static char oneliner[] = "Dropped datagrams count";
                static char full_description[] = 
                    "Number of datagrams/packets that the agent has dropped\n"
                    "during its lifetime, due to either being unable to parse the data \n"
                    "or semantically incorrect values.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 3:
            {
                static char oneliner[] = "Aggregated datagrams count";
                static char full_description[] = 
                    "Number of datagrams/packets that the agent has aggregated\n"
                    "during its lifetime (that is, that were processed fully).\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 4:
            {
                static char oneliner[] = "Number of tracked metrics";
                static char full_description[] = 
                    "Number of tracked metrics.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 5:
            {
                static char oneliner[] = "Total time in microseconds spent parsing metrics";
                static char full_description[] = 
                    "Total time in microseconds spent parsing metrics.\nIncludes time spent parsing a datagram and failing midway.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 6:
            {
                static char oneliner[] = "Total time in microseconds spent aggregating metrics";
                static char full_description[] = 
                    "Total time in microseconds spent aggregating metrics.\nIncludes time spent aggregating a metric and failing midway.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 7:
            {
                static char oneliner[] = "Maximum UDP packet size";
                static char full_description[] = 
                    "Maximum UDP packet size. This shows current setting.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 8:
            {
                static char oneliner[] = "Maximum size of unprocessed packets Q";
                static char full_description[] = 
                    "Maximum size of unprocessed packets Q. This shows current setting.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 9:
            {
                static char oneliner[] = "Verbosity flag.";
                static char full_description[] = 
                    "Verbosity flag. This shows current setting.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 10:
            {
                static char oneliner[] = "Debug flag.";
                static char full_description[] = 
                    "Debug flag. This shows current setting.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 11:
            {
                static char oneliner[] = "Debug output filename.";
                static char full_description[] = 
                    "Debug output filename. This shows current setting.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 12:
            {
                static char oneliner[] = "Port that is listened to.";
                static char full_description[] = 
                    "Port that is listened to. This shows current setting.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 13:
            {
                static char oneliner[] = "Used parser type.";
                static char full_description[] = 
                    "Used parser type. This shows current setting.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
                return 0;
            }
            case 14: 
            {
                static char oneliner[] = "Used duration aggregation type.";
                static char full_description[] = 
                    "Used duration aggregation type. This shows current setting.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
        }
        return PM_ERR_PMID;
    }
    return PM_ERR_TEXT;
}

/**
 * Wrapper around pmdaInstance, called before control is passed to pmdaInstance
 * @arg in_dom - Instance domain description
 * @arg inst - Instance domain num
 * @arg name - Instance domain name
 * @arg result - Result to populate
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_instance(pmInDom in_dom, int inst, char* name, pmInResult** result, pmdaExt* pmda) {
    statsd_possible_reload(pmda);
    return pmdaInstance(in_dom, inst, name, result, pmda);
}

/**
 * Wrapper around pmdaFetch, called before control is passed to pmdaFetch
 * @arg num_pm_id - Metric id
 * @arg pm_id_list - Collection of instance domains
 * @arg resp - Result to populate
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_fetch(int num_pm_id, pmID pm_id_list[], pmResult** resp, pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*) pmdaExtGetData(pmda);
    statsd_possible_reload(pmda);
    if (data->notify) {
        pmdaExtSetFlags(pmda, data->notify);
        data->notify = 0;
    }
    return pmdaFetch(num_pm_id, pm_id_list, resp, pmda);
}

/**
 * Wrapper around pmdaTreePMID, called before control is passed to pmdaTreePMID
 * @arg name -
 * @arg pm_id - Metric identifier
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_pmid(const char* name, pmID* pm_id, pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData(pmda);
    statsd_possible_reload(pmda);
    return pmdaTreePMID(data->pcp_pmns, name, pm_id);
}

/**
 * Wrapper around pmdaTreeName, called before control is passed to pmdaTreeName
 * @arg pm_id - Metric identifier
 * @arg nameset -
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_name(pmID pm_id, char*** nameset, pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData(pmda);
    statsd_possible_reload(pmda);
    return pmdaTreeName(data->pcp_pmns, pm_id, nameset);
} 

/**
 * Wrapper around pmdaTreeChildren, called before control is passed to pmdaTreeChildren
 * @arg name - metric name
 * @arg traverse - traverse flag
 * @arg children - descendant metrics
 * @arg status - descendant status
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_children(const char* name, int traverse, char*** children, int** status, pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData(pmda);
    statsd_possible_reload(pmda);
    return pmdaTreeChildren(data->pcp_pmns, name, traverse, children, status);
}

/**
 * Wrapper around pmdaLabel, called before control is passed to pmdaLabel
 * - since this is always called before statsd_label_callback, we set global *g_ext_reference* so that it is available in statsd_label_callback as well 
 * @arg ident - identifier
 * @arg type - type specifying what is identifying
 * @arg lp - Provides name and value indexes in JSON string
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
static pmdaExt* g_ext_reference;
int
statsd_label(int ident, int type, pmLabelSet** lp, pmdaExt* pmda) {
    g_ext_reference = pmda;
    statsd_possible_reload(pmda);
    return pmdaLabel(ident, type, lp, pmda);
}

/**
 * Maps labels from metric_label to their instance domains and adds them to resulting output 
 * @arg in_dom - Instance domain description
 * @arg inst - Instance
 * @arg lp - Provides name and value indexes in JSON string
 */
int
statsd_label_callback(pmInDom in_dom, unsigned int inst, pmLabelSet** lp) {
    int is_static_domain =  pmInDom_serial(in_dom) == STATSD_METRIC_DEFAULT_INDOM ||
                            pmInDom_serial(in_dom) == STATSD_METRIC_DEFAULT_DURATION_INDOM ||
                            pmInDom_serial(in_dom) == STATS_METRIC_COUNTERS_INDOM;
    if (is_static_domain) {
        return 0;
    }
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData(g_ext_reference);
    dictEntry* entry = dictFind(data->instance_map, pmInDomStr(in_dom));
    if (entry == NULL) {
        return 0;
    }
    char* metric_key = (char*)entry->v.val;
    struct metric* item;
    int metric_found = find_metric_by_name(data->metrics_storage, metric_key, &item);
    if (!metric_found) {
        return 0;
    }
    int metric_has_root_value = item->value != NULL;
    int instance_label_offset;
    if (item->type == METRIC_TYPE_COUNTER ||
        item->type == METRIC_TYPE_GAUGE) {
        if (metric_has_root_value && inst == 0) {
            return 0;
        }
        instance_label_offset = metric_has_root_value ? inst - 1 : inst;
    } else {
        if (metric_has_root_value && inst < 9) {
            return 0;
        }
        int instance_group = inst / 9;
        instance_label_offset = metric_has_root_value ? instance_group - 1 : instance_group;
    }
    char* label_key = item->meta->pcp_instance_map->labels[instance_label_offset];
    struct metric_label* label;
    int found = find_label_by_name(
        data->metrics_storage,
        item,
        label_key,
        &label
    );
    if (!found) {
        return 0;
    }
    pthread_mutex_lock(&data->metrics_storage->mutex);
    pmdaAddLabels(lp, "%s", label->labels);
    pthread_mutex_unlock(&data->metrics_storage->mutex);
    return label->pair_count;
}

/**
 * Handles fetches of all static metrics - all metrics about agent itself in statsd.pmda.* namespace
 * @arg mdesc - pmdaMetric being fetched
 * @arg instance - metric's instance domain's instance
 * @arg atom - Placeholder to be populated with response
 * @return status, PMDA_FETCH_STATIC on success, PMDA_ERR_INST for incorrect instance identifier and PMDA_ERR_PMID for incorrect metric identifier
 */
static int
statsd_resolve_static_metric_fetch(pmdaMetric* mdesc, unsigned int instance, pmAtomValue** atom) {
    struct pmda_metric_helper* helper = (struct pmda_metric_helper*) mdesc->m_user;
    struct pmda_data_extension* data = helper->data;
    struct agent_config* config = data->config;
    struct pmda_stats_container* stats = data->stats_storage;
    unsigned int item = pmID_item(mdesc->m_desc.pmid);
    int status = PMDA_FETCH_STATIC;
    switch (item) {
        /* received */
        case 0:
            (*atom)->ull = get_agent_stat(config, stats, STAT_RECEIVED, NULL);
            break;
        /* parsed */
        case 1:
            (*atom)->ull = get_agent_stat(config, stats, STAT_PARSED, NULL);
            break;
        /* thrown away */
        case 2:
            (*atom)->ull = get_agent_stat(config, stats, STAT_DROPPED, NULL);
            break;
        /* aggregated */
        case 3:
            (*atom)->ull = get_agent_stat(config, stats, STAT_AGGREGATED, NULL);
            break;
        case 4:
        {
            if (instance == 0) {
                (*atom)->ull = get_agent_stat(config, stats, STAT_TRACKED_METRIC, (void*)METRIC_TYPE_COUNTER);
                break;
            }
            if (instance == 1) {
                (*atom)->ull = get_agent_stat(config, stats, STAT_TRACKED_METRIC, (void*)METRIC_TYPE_GAUGE);
                break;
            }
            if (instance == 2) {
                (*atom)->ull = get_agent_stat(config, stats, STAT_TRACKED_METRIC, (void*)METRIC_TYPE_DURATION);
                break;
            }
            if (instance == 3) {
                (*atom)->ull = get_agent_stat(config, stats, STAT_TRACKED_METRIC, NULL);
                break;
            }
            status = PM_ERR_INST;
            break;
        }
        /* time_spent_parsing */
        case 5:
            (*atom)->ull = get_agent_stat(config, stats, STAT_TIME_SPENT_PARSING, NULL);
            break;
        /* time_spent_aggregating */
        case 6:
            (*atom)->ull = get_agent_stat(config, stats, STAT_TIME_SPENT_AGGREGATING, NULL);
            break;

        /* settings.max_udp_packet_size */
        case 7:
            (*atom)->ull = (__uint64_t) config->max_udp_packet_size;
            break;
        /* settings.max_unprocessed_packets */
        case 8:
            (*atom)->ul = config->max_unprocessed_packets;
            break;
        /* settings.verbose */
        case 9:
            (*atom)->ul = config->verbose;
            break;
        /* settings.debug_output_filename */
        case 10:
        {
            char* result = strdup(config->debug_output_filename);
            ALLOC_CHECK(result, "Unable to allocate memory for port value.");
            (*atom)->cp = result;
            status = PMDA_FETCH_DYNAMIC;
            break;
        }
        /* settings.port */
        case 11:
            (*atom)->ul = config->port;
            break;
        /* settings.parser_type */
        case 12:
        {   
            const char* basic = "Basic";
            const char* ragel = "Ragel";
            if (config->parser_type == PARSER_TYPE_BASIC) {
                (*atom)->cp = (char*)basic;
            } else {
                (*atom)->cp = (char*)ragel;
            }
            break;
        }
        /* settings.duration_aggregation_type */
        case 13:
        {
            const char* basic = "Basic";
            const char* hdr = "HDR histogram";
            if (config->duration_aggregation_type == DURATION_AGGREGATION_TYPE_BASIC) {
                (*atom)->cp = (char*)basic;
            } else {
                (*atom)->cp = (char*)hdr;
            }
            break;
        }
        default:
            status = PM_ERR_PMID;
    }
    return status;
}

/**
 * Handles fetches of all dynamic metrics - all metrics aggregated via StatsD in statsd.* namespace, excluding blocklisted ones in statsd.pmda.*
 * @arg mdesc - pmdaMetric being fetched
 * @arg instance - metric's instance domain's instance
 * @arg atom - Placeholder to be populated with response
 * @return status, PMDA_FETCH_STATIC on success, PMDA_ERR_INST for incorrect instance identifier and PMDA_ERR_PMID for incorrect metric identifier
 */
static int
statsd_resolve_dynamic_metric_fetch(pmdaMetric* mdesc, unsigned int instance, pmAtomValue** atom) {
    struct pmda_metric_helper* helper = (struct pmda_metric_helper*) mdesc->m_user;
    struct pmda_data_extension* data = helper->data;
    struct agent_config* config = data->config;
    struct metric* result = helper->item;
    unsigned int serial = pmInDom_serial(mdesc->m_desc.indom);
    int is_default_domain = (serial == STATSD_METRIC_DEFAULT_INDOM) ||
                            (serial == STATSD_METRIC_DEFAULT_DURATION_INDOM);
    int status = PM_ERR_INST;
    enum DURATION_INSTANCE duration_stat;
    // metrics without any labels
    if (is_default_domain) {
        pthread_mutex_lock(&data->metrics_storage->mutex);
        if (result->type == METRIC_TYPE_DURATION) {
            duration_stat = map_to_duration_instance(instance);
            (*atom)->d = get_duration_instance(config, result->value, duration_stat);
        } else {        
            (*atom)->d = *(double*)result->value;
        }
        status = PMDA_FETCH_STATIC;
        pthread_mutex_unlock(&data->metrics_storage->mutex);
    } 
    // metrics with labels
    else {
        int metric_has_root_value = result->value != NULL;
        int request_for_root_value = metric_has_root_value && 
                                    ((result->type == METRIC_TYPE_DURATION && instance < 9) || instance == 0);
        // check if request was for root value
        if (request_for_root_value) {
            pthread_mutex_lock(&data->metrics_storage->mutex);
            if (result->type == METRIC_TYPE_DURATION) {
                duration_stat = map_to_duration_instance(instance);
                (*atom)->d = get_duration_instance(config, result->value, duration_stat);
            } else {        
                (*atom)->d = *(double*)result->value;
            }
            status = PMDA_FETCH_STATIC;
            pthread_mutex_unlock(&data->metrics_storage->mutex);
        } else {
        // else return some labeled value
            int instance_label_offset;
            if (result->type == METRIC_TYPE_DURATION) {
                int instance_group = instance / 9;
                instance_label_offset = metric_has_root_value ? instance_group - 1 : instance_group;
            } else {
                instance_label_offset = metric_has_root_value ? instance - 1 : instance;
            }
            char* label_key = result->meta->pcp_instance_map->labels[instance_label_offset];
            struct metric_label* label;
            int found = find_label_by_name(
                helper->data->metrics_storage,
                result,
                label_key,
                &label
            );
            if (found) {
                pthread_mutex_lock(&data->metrics_storage->mutex);
                if (result->type == METRIC_TYPE_DURATION) {
                    duration_stat = map_to_duration_instance(instance);
                    (*atom)->d = get_duration_instance(config, label->value, duration_stat);
                } else {
                    (*atom)->d = *(double*)label->value;
                }
                status = PMDA_FETCH_STATIC;
                pthread_mutex_unlock(&data->metrics_storage->mutex);
            }
        }
    }
    return status;
}

/**
 * This callback deals with one request unit which may be part of larger request of PDU_FETCH
 * @arg pmdaMetric - requested metric, along with user data, in out case PMDA extension structure (contains agent-specific private data)
 * @arg inst - requested metric instance
 * @arg atom - atom that should be populated with request response
 * @return value less then 0 signalizes error, equal to 0 means that metric is not available, greater then 0 is success
 */
int
statsd_fetch_callback(pmdaMetric* mdesc, unsigned int instance, pmAtomValue* atom) {
    unsigned int cluster = pmID_cluster(mdesc->m_desc.pmid);
    int status;
    switch (cluster) {
        /* cluster 0 is reserved for stats - info about agent itself */
        case 0:
            status = statsd_resolve_static_metric_fetch(mdesc, instance, &atom);
            break;
        default:
            status = statsd_resolve_dynamic_metric_fetch(mdesc, instance, &atom);
    }
    return status;
}
