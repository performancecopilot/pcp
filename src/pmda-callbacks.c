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
 * Gets next valid pmID
 * - used for getting pmIDs for yet unrecorded metrics
 * @arg data - PMDA extension structure (contains agent-specific private data)
 * @return new valid pmID
 */
static pmID
get_next_pmID(struct pmda_data_extension* data) {
    pmID next = pmID_build(STATSD, data->next_cluster_id, data->next_item_id);
    if (data->next_cluster_id >= (1 << 12)) {
        DIE("Agent ran out of metric ids.");
    }
    if (data->next_item_id == 1000) {
        data->next_item_id = 0;
        data->next_cluster_id += 1;
    } else {
        data->next_item_id += 1;
    }
    return next;
}

/**
 * Resets internal counters that track current id's
 * @arg data - PMDA extension structure (contains agent-specific private data)
 */
static void
reset_pmID_counters(struct pmda_data_extension* data) {
    data->next_item_id = 0;
    data->next_cluster_id = 1;
}

/**
 * Adds new metric to pcp metric table and pcp pmns and then increments total metric count
 * @arg key  - Key under which metric is saved in hashtable
 * @arg item - StatsD Metric from which to extract what PCP Metric to create, assigns PMID and PCP name to metric as a side-effect
 * @arg data - PMDA extension structure (contains agent-specific private data)
 */
static void
create_pcp_metric(char* key, struct metric* item, pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData((pmdaExt*)pmda);
    // set pmid to metric
    item->meta->pmid = get_next_pmID(data);
    // set pcp_name to metric
    char name[64];
    size_t len = pmsprintf(name, 64, "statsd.%s", item->name) + 1;
    item->meta->pcp_name = (char*) malloc(sizeof(char) * len);
    ALLOC_CHECK("Unable to allocate mem for PCP metric name for StatsD metric.");
    memcpy((char*)item->meta->pcp_name, name, len);
    VERBOSE_LOG("statsd: create_metric: %s - %s", item->meta->pcp_name, pmIDStr(item->meta->pmid));
    // extend current pcp_metrics
    data->pcp_metrics = realloc(data->pcp_metrics, sizeof(pmdaMetric) * (data->pcp_metric_count + 1));
    ALLOC_CHECK("Cannot grow statsd metric list");
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
    if (item->type == METRIC_TYPE_DURATION) {
        new_metric->m_desc.indom = pmInDom_build(((pmdaExt*)pmda)->e_domain, DURATION_INDOM);
    } else {
        new_metric->m_desc.indom = PM_INDOM_NULL;
    }
    new_metric->m_desc.sem = PM_SEM_INSTANT;
    memset(&new_metric->m_desc.units, 0, sizeof(pmUnits));
    DEBUG_LOG(
        "STATSD: adding metric %s %s from %s\n", item->meta->pcp_name, pmIDStr(item->meta->pmid), item->name
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
metric_foreach_callback(char* key, struct metric* item, void* pmda) {
    // this prevents creating of metrics/labels that have tags as we don't deal with those yet
    if (item->meta->tags != NULL || item->meta->sampling) return;
    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData((pmdaExt*)pmda);
    // lets check if metric already has pmid assigned, if so it has PCP name as well, then just insert it into tree
    if (item->meta->pmid == PM_ID_NULL) {
        create_pcp_metric(key, item, (pmdaExt*)pmda);
    }
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
            pthread_mutex_lock(&data->metrics_storage->mutex);
            (*atom)->d = *(double*)result->value;
            status = PMDA_FETCH_STATIC;
            pthread_mutex_unlock(&data->metrics_storage->mutex);
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
        {
            status = PMDA_FETCH_STATIC;
            switch (item) {
                /* received */
                case 0:
                    atom->ull = get_agent_stat(config, stats, STAT_RECEIVED, NULL);
                    break;
                /* parsed */
                case 1:
                    atom->ull = get_agent_stat(config, stats, STAT_PARSED, NULL);
                    break;
                /* thrown away */
                case 2:
                    atom->ull = get_agent_stat(config, stats, STAT_DROPPED, NULL);
                    break;
                /* aggregated */
                case 3:
                    atom->ull = get_agent_stat(config, stats, STAT_AGGREGATED, NULL);
                    break;
                case 4:
                {
                    if (instance == 0) {
                        atom->ull = get_agent_stat(config, stats, STAT_TRACKED_METRIC, (void*)METRIC_TYPE_COUNTER);
                        break;
                    }
                    if (instance == 1) {
                        atom->ull = get_agent_stat(config, stats, STAT_TRACKED_METRIC, (void*)METRIC_TYPE_GAUGE);
                        break;
                    }
                    if (instance == 2) {
                        atom->ull = get_agent_stat(config, stats, STAT_TRACKED_METRIC, (void*)METRIC_TYPE_DURATION);
                        break;
                    }
                    if (instance == 3) {
                        atom->ull = get_agent_stat(config, stats, STAT_TRACKED_METRIC, NULL);
                        break;
                    }
                    status = PM_ERR_INST;
                    break;
                }
                /* time_spent_parsing */
                case 5:
                    atom->ull = get_agent_stat(config, stats, STAT_TIME_SPENT_PARSING, NULL);
                    break;
                /* time_spent_aggregating */
                case 6:
                    atom->ull = get_agent_stat(config, stats, STAT_TIME_SPENT_AGGREGATING, NULL);
                    break;
                default:
                    status = PM_ERR_PMID;
            }
            break;
        }
        default:
            status = statsd_resolve_dynamic_metric_fetch(mdesc, instance, &atom);
    }
    return status;
}
