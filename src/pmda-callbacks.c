#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <hdr/hdr_histogram.h>

#include "pmdastatsd.h"
#include "aggregator-metrics.h"
#include "aggregator-stats.h"
#include "aggregator-metric-duration-exact.h"
#include "utils.h"
#include "config-reader.h"
#include "pmda-callbacks.h"
#include "../domain.h"

/**
 * Gets next valid pmInDom
 * - used for getting pmInDoms for yet unrecorded metrics
 * @arg data - 
 */
static pmInDom
get_next_pmInDom() {
    static int next_pmindom = 3; 
    pmInDom next = pmInDom_build(STATSD, next_pmindom);
    if (next_pmindom - 1 == (1 << 22)) {
        DIE("Agent ran out of metric instance domains.");
    }
    next_pmindom += 1;
    return next;
}

/**
 * Adds new metric to pcp metric table and pcp pmns and then increments total metric count
 * @arg key  - Key under which metric is saved in hashtable
 * @arg item - StatsD Metric from which to extract what PCP Metric to create, assigns PMID and PCP name to metric as a side-effect
 * @arg data - PMDA extension structure (contains agent-specific private data)
 */
static void
create_pcp_metric(char* key, struct metric* item, void* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData((pmdaExt*)pmda);    
    data->pcp_metrics = realloc(data->pcp_metrics, sizeof(pmdaMetric) * (data->pcp_metric_count + 1));
    ALLOC_CHECK("Cannot grow statsd metric list.");
    // .. with new metric description 
    size_t i = data->pcp_metric_count;
    pmdaMetric* new_metric = &data->pcp_metrics[i];
    struct pmda_metric_helper* helper = (struct pmda_metric_helper*) malloc(sizeof(struct pmda_metric_helper));
    ALLOC_CHECK("Unable to allocate mem for metric helper struct.");
    helper->key = key;
    helper->item = item;
    helper->data = data;
    new_metric->m_user = helper;
    new_metric->m_desc.pmid = item->meta->pmid;
    new_metric->m_desc.type = PM_TYPE_DOUBLE;
    new_metric->m_desc.indom = item->meta->pmindom;
    new_metric->m_desc.sem = PM_SEM_INSTANT;
    memset(&new_metric->m_desc.units, 0, sizeof(pmUnits));
    DEBUG_LOG(
        "STATSD: adding metric %s %s from %s\n", item->meta->pcp_name, pmIDStr(item->meta->pmid), item->name
    );
    item->meta->pcp_metric_created = 1;
    item->meta->pcp_metric_index = i;
}

static void
create_new_instance(char* key, struct metric* item, void* pmda) {
    // struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData((pmdaExt*)pmda);
    // // get new pmInDom
    // pmInDom next_pmindom = get_next_pmInDom();
    // // add new pmdaIndom to domain list
    // data->pcp_instance_domains = 
    //     (pmdaIndom*) realloc(
    //         data->pcp_instance_domains,
    //         (data->pcp_instance_domain_count + 1) * sizeof(pmdaIndom)
    //     );
    // ALLOC_CHECK("Unable to resize memory for PMDA instance domains");
    // // build new pmdaInstid
    // size_t indom_i = data->pcp_instance_domain_count;
    // size_t labels_count = item->children->ht[0].size; // this is never 0
    // size_t indom_i_inst_cnt;
    // if (item->type == METRIC_TYPE_DURATION) {
    //     // because duration has 9 instances per metric/metric_label record
    //     indom_i_inst_cnt = (labels_count + 1) * 9;
    // } else {
    //     // because counter/gauge have 1 instance per metric/metric_label record
    //     indom_i_inst_cnt = (labels_count + 1);
    // }
    // pmdaInstid* instances =
    //     (pmdaInstid*) malloc(sizeof(pmdaInstid) * indom_i_inst_cnt);
    // ALLOC_CHECK("Unable to allocate memory for new PMDA instance domain instances.");
    // if (item->type == METRIC_TYPE_DURATION) {
    //     // reuse static pmdaInstid* instances from STATSD_METRIC_DEFAULT_DURATION_INDOM domain
    //     pmdaInstid* static_instances = data->pcp_instance_domains[STATSD_METRIC_DEFAULT_DURATION_INDOM];
    //     size_t hardcoded_inst_desc_cnt = 9;
    //     size_t i;
    //     for (i = 0; i < hardcoded_inst_desc_cnt; i++) {
    //         instances[i] = static_instances[i];
    //     }
    // } else {
    //     // reuse static pmdaInstid* instances from STATSD_METRIC_DEFAULT_INDOM domain
    //     instances[0] = data->pcp_instance_domains[STATSD_METRIC_DEFAULT_INDOM].it_set;
    // }
    // // iterate over metric labels and fill remaining instances, recorded order in map on metric struct which will contain references to equivalent metric_label hashtable keys
    // size_t label_index = 0;
    // // - prepare map
    // item->meta->pcp_instance_map =
    //     (struct pmdaInstid_map*) malloc(sizeof(pmdaInstid_map));
    // ALLOC_CHECK("Unable to allocate memory for new instance domain map.");
    // item->meta->pcp_instance_map->length = labels_count;
    // item->meta->pcp_instance_map->labels = (char**) malloc(sizeof(char*) * labels_count);
    // ALLOC_CHECK("Unable to allocate memory for new instance domain map label references.");
    // // - prepare for iteration
    // char buffer[100];
    // size_t instance_name_length;
    // //   - prepare format strings for duration instances
    // static char* duration_metric_instance_keywords[] = {
    //     "/min::%s",
    //     "/max::%s",
    //     "/median::%s",
    //     "/average::%s",
    //     "/percentile90::%s",
    //     "/percentile95::%s",
    //     "/percentile99::%s",
    //     "/count::%s",
    //     "/std_deviation::%s"
    // }
    // static size_t keywords_count = 9;
    // // - iterate
    // dictIterator* iterator = dictGetSafeIterator(item->children);
    // dictEntry* current;
    // while ((current = dictNext(iterator)) != NULL) {
    //     struct metric_label* label = (struct metric_label*)current->v.val;
    //     // store on which instance domain instance index we find current label
    //     item->meta->pcp_instance_map->labels[label_index] = label->labels;
    //     if (label->type == METRIC_TYPE_DURATION) {
    //         // duration metric has 9 instances per single metric_label, formatted with premade label string segments
    //         size_t i = 0;
    //         int label_offset = (label_index + 1) * 9;
    //         for (i = 0; i < keywords_count; i++) {
    //             instances[label_offset + i].i_inst = label_offset + i;
    //             instance_name_length = pmsprintf(buffer, 100, duration_metric_instance_keywords[i], label->meta->instance_label_segment_str);
    //             memcpy(instances[label_offset + i].i_name, buffer, instance_name_length);
    //         }
    //     } else {
    //         // counter/gauge has 1 instance per single metric_label, just add / to premade label string
    //         instances[label_index + 1].i_inst = label_index + 1;
    //         instance_name_length = pmsprintf(buffer, 100, "/%s", label->meta->instance_label_segment_str);
    //         memcpy(instances[label_index + 1].i_name, buffer, instance_name_length);
    //     }
    //     label_index++;
    // }
    // // populate new instance domain
    // data->pcp_instance_domains[indom_i].it_indom = next_pmindom;
    // data->pcp_instance_domains[indom_i].it_numinst = indom_i_inst_cnt;
    // data->pcp_instance_domains[indom_i].it_set = instances;
    // // current metric instance domain has changed
    // item->meta->pmindom = next_pmindom;
    // // save on which index instance domain for current metric is stored
    // item->meta->pcp_instance_domain_index = indom_i;
    // // we added new instance domain
    // data->pcp_instance_domain_count += 1;
}

static void
update_pcp_metric_instance_domain(char* key, struct metric* item, void* pmda) {
    // no reason to update if metric has no labels
    if (item->children == NULL) return;
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData((pmdaExt*)pmda);
    // these are only for comparison
    pmInDom statsd_metric_default_duration_indom = pmInDom_build(STATSD, STATSD_METRIC_DEFAULT_DURATION_INDOM);
    pmInDom statsd_metric_default_indom = pmInDom_build(STATSD, STATSD_METRIC_DEFAULT_INDOM);
    if (item->meta->pmindom == statsd_metric_default_duration_indom ||
        item->meta->pmindom == statsd_metric_default_indom) {
        create_new_instance(key, item, pmda);
    } else {
        // TODO: we need to update existing instance domain record

    }
    // Change request was processed
    item->meta->pcp_instance_change_requested = 0;
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
metric_foreach_callback(char* key, struct metric* item, void* pmda) {
    // skip metric if its still being processed (case when new metric gets added and datagram has only label, but metric addition and label appending are 2 separate actions)
    if (item->pernament == 0) return;
    // this prevents creating of metrics/labels that have tags as we don't deal with those yet
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData((pmdaExt*)pmda);
    // lets check if metric already has pmid assigned, if so it has PCP name as well, then just insert it into tree
    if (item->meta->pcp_metric_created == 0) {
        create_pcp_metric(key, item, (pmdaExt*)pmda);
    }
    // pcp_instance_change_requested flag is set, when metric gets new metric_label
    // if (item->meta->pcp_instance_change_requested == 1) {
    //     update_pcp_metric_instance_domain(key, item, (pmdaExt*)pmda);
    // }
    pmdaTreeInsert(data->pcp_pmns, item->meta->pmid, item->meta->pcp_name);
    process_stat(data->config, data->stats_storage, STAT_TRACKED_METRIC, (void*)item->type);
    data->pcp_metric_count += 1;
}

/**
 * Maps all stats (both hardcoded and the ones aggregated from StatsD datagrams) to 
 */
static void
statsd_map_stats(pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*) pmdaExtGetData(pmda);
    char name[64];
    int status = 0;
    if (data->pcp_pmns) {
        pmdaTreeRelease(data->pcp_pmns);
        data->notify |= PMDA_EXT_NAMES_CHANGE; 
    }
    status = pmdaTreeCreate(&data->pcp_pmns);
    if (status < 0) {
        pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n", pmGetProgname(), pmErrStr(status));
        data->pcp_pmns = NULL;
        return;
    } else {
        reset_stat(data->config, data->stats_storage, STAT_TRACKED_METRIC);
    }
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
    data->pcp_metric_count = 7;    
    iterate_over_metrics(data->metrics_storage, metric_foreach_callback, pmda);
    pmdaTreeRebuildHash(data->pcp_pmns, data->pcp_metric_count);
    pthread_mutex_lock(&data->metrics_storage->mutex);
    data->generation = data->metrics_storage->generation;
    pthread_mutex_unlock(&data->metrics_storage->mutex);
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
        DEBUG_LOG("statsd: %s: reloading", pmGetProgname());
        statsd_map_stats(pmda);
        pmda->e_indoms = data->pcp_instance_domains;
        pmda->e_nindoms = data->pcp_instance_domain_count;
        pmdaRehash(pmda, data->pcp_metrics, data->pcp_metric_count);
        DEBUG_LOG(
            "statsd: %s: %lu metrics and %lu instance domains after reload",
            pmGetProgname(),
            data->pcp_metric_count,
            data->pcp_instance_domain_count
        );
    }
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
                    "Total time in microseconds spent parsing metrics. Includes time spent parsing a datagram and failing midway.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 6:
            {
                static char oneliner[] = "Total time in microseconds spent aggregating metrics";
                static char full_description[] = 
                    "Total time in microseconds spent aggregating metrics. Includes time spent aggregating a metric and failing midway.\n";
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
 * Wrapper around pmdaStore, called before control is passed to pmdaStore
 * @arg result - Result to me populated
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_store(pmResult* result, pmdaExt* pmda) {
    (void)result;
    statsd_possible_reload(pmda);
    return PM_ERR_PMID;
}

/**
 * Wrapper around pmdaTreePMID, called before control is passed to pmdaTreePMID
 * @arg name -
 * @arg pm_id - Instance domain
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
 * @arg pm_id - Instance domain
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
 * NOT IMPLEMENTED
 * Wrapper around pmdaLabel, called before control is passed to pmdaLabel
 * @arg ident - 
 * @arg type - 
 * @arg lp - Provides name and value indexes in JSON string
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_label(int ident, int type, pmLabelSet** lp, pmdaExt* pmda) {
    switch (type) {
        case PM_LABEL_DOMAIN:
            break;
        case PM_LABEL_CLUSTER:
            break;
        case PM_LABEL_ITEM:
            break;
        case PM_LABEL_INDOM:
            break;
        case PM_LABEL_INSTANCES:
            break;
    }
    statsd_possible_reload(pmda);
    return pmdaLabel(ident, type, lp, pmda);
}

/**
 * NOT IMPLEMENTED
 * @arg in_dom - Instance domain description
 * @arg inst -
 * @arg lp - Provides name and value indexes in JSON string
 */
int
statsd_label_callback(pmInDom in_dom, unsigned int inst, pmLabelSet** lp) {
    (void)in_dom;
    (void)inst;
    (void)lp;
    return 0;
}

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
        default:
            status = PM_ERR_PMID;
    }
    return status;
}

static int
statsd_resolve_dynamic_metric_fetch(pmdaMetric* mdesc, unsigned int instance, pmAtomValue** atom) {
    struct pmda_metric_helper* helper = (struct pmda_metric_helper*) mdesc->m_user;
    struct pmda_data_extension* data = helper->data;
    struct agent_config* config = data->config;
    struct metric* result = helper->item;
    int status;
    static struct duration_values_meta meta;
    // now return response based on metric type
    switch (result->type) {
        // counter and gauge simply pass value to PCP
        case METRIC_TYPE_COUNTER:
        case METRIC_TYPE_GAUGE:
            status = PM_ERR_INST;
            // this needs to be casted to __pmInDom_int, but I don't know how right now
            if ((mdesc->m_desc.indom & 0x2FFFFF) == STATSD_METRIC_DEFAULT_INDOM) {
                if (instance == 0) {
                    pthread_mutex_lock(&data->metrics_storage->mutex);
                    (*atom)->d = *(double*)result->value;
                    status = PMDA_FETCH_STATIC;
                    pthread_mutex_unlock(&data->metrics_storage->mutex);
                }
            } 
            break;
        // duration passes values trough instances
        case METRIC_TYPE_DURATION:
            pthread_mutex_lock(&data->metrics_storage->mutex);
            status = PMDA_FETCH_STATIC;
            switch (instance) {
                case 0:
                    (*atom)->d = get_duration_instance(config, result, DURATION_MIN);
                    break;
                case 1:
                    (*atom)->d = get_duration_instance(config, result, DURATION_MAX);
                    break;
                case 2:
                    (*atom)->d = get_duration_instance(config, result, DURATION_MEDIAN);
                    break;
                case 3:
                    (*atom)->d = get_duration_instance(config, result, DURATION_AVERAGE);
                    break;
                case 4:
                    (*atom)->d = get_duration_instance(config, result, DURATION_PERCENTILE90);
                    break;
                case 5:
                    (*atom)->d = get_duration_instance(config, result, DURATION_PERCENTILE95);
                    break;
                case 6:
                    (*atom)->d = get_duration_instance(config, result, DURATION_PERCENTILE99);
                    break;
                case 7:
                    (*atom)->d = get_duration_instance(config, result, DURATION_COUNT);
                    break;
                case 8:
                    (*atom)->d = get_duration_instance(config, result, DURATION_STANDARD_DEVIATION);
                    break;
                default:
                    status = PM_ERR_INST;
            }
            pthread_mutex_unlock(&data->metrics_storage->mutex);            
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
    struct pmda_metric_helper* helper = (struct pmda_metric_helper*) mdesc->m_user;
    struct pmda_data_extension* data = helper->data;
    struct agent_config* config = data->config;
    struct pmda_stats_container* stats = data->stats_storage;
    struct pmda_metrics_container* metrics = data->metrics_storage;
    unsigned int cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int item = pmID_item(mdesc->m_desc.pmid);
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
