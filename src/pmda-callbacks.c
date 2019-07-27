#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "statsd.h"
#include "aggregator-metrics.h"
#include "aggregator-stats.h"
#include "aggregator-metric-duration-exact.h"
#include "utils.h"
#include "config-reader.h"
#include "pmda-callbacks.h"
#include "../domain.h"

#define STATSD_MAX_COUNT 10000
#define STATSD_MAX_CLUSTER ((1<<12)-1)

static int g_cluster_num = 1;
static int g_item_num = 0;

/*
 * Check cluster number validity (must be in range 0 .. 1<<12).
 */
static pmID
get_next_pmID(struct pmda_data_extension* data) {
    pmID next = pmID_build(STATSD, data->next_cluster_id, data->next_item_id);
    if (data->next_cluster_id >= (1 << 12)) {
        DIE("Run out of cluster ids.");
    }
    if (data->next_item_id == 1000) {
        data->next_item_id = 0;
        data->next_cluster_id += 1;
    } else {
        data->next_item_id += 1;
    }
    return next;
}

static void
reset_pmID_counters(struct pmda_data_extension* data) {
    data->next_item_id = 0;
    data->next_cluster_id = 1;
}

static void
create_metric_iteration_callback(struct metric* item, void* pmda) {
    // this prevents creating of metrics that have tags or sampling as we don't deal with those yet
    if (item->meta != NULL) return;

    struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData((pmdaExt*)pmda);
    dictEntry* result = dictFind(data->pcp_metric_reverse_lookup, item->name);

    // return if record of metric exists
    if (result != NULL) return;

    // setup new pmID
    pmID pmid = get_next_pmID(data); 
    // setup new reverse lookup record
    struct pcp_reverse_lookup_record* new_record =
        (struct pcp_reverse_lookup_record*) malloc(sizeof(pcp_reverse_lookup_record));
    ALLOC_CHECK("Unable to create new reverse lookup record.");
    new_record->name = item->name;
    const char* new_key = pmIDStr(pmid);
    if (dictAdd(data->pcp_metric_reverse_lookup, (void*)new_key, new_record) != 0) {
        DEBUG_LOG("Unable to create new reverse lookup record.");
    }

    // setup new metric name    
    char name[64];
    pmsprintf(name, 64, "statsd.%s", item->name);
    DEBUG_LOG("statsd: create_metric: %s - %s", name, pmIDStr(pmid));

    // extend current pcp_metrics
    data->pcp_metrics = realloc(data->pcp_metrics, sizeof(pmdaMetric) * (data->pcp_metric_count + 1));
    ALLOC_CHECK("Cannot grow statsd metric list");
    // .. with new metric description 
    size_t new_item_index = data->pcp_metric_count;
    pmdaMetric* new_metric = &data->pcp_metrics[new_item_index];
    new_metric->m_user = data;
    new_metric->m_desc.pmid = pmid;
    if (item->type == METRIC_TYPE_DURATION) {
        new_metric->m_desc.type = PM_TYPE_DOUBLE;
        new_metric->m_desc.indom = pmInDom_build(((pmdaExt*)pmda)->e_domain, DURATION_INDOM);
    } else {
        new_metric->m_desc.type = PM_TYPE_DOUBLE;
        new_metric->m_desc.indom = PM_INDOM_NULL;
    }
    new_metric->m_desc.sem = PM_SEM_INSTANT;
    memset(&new_metric->m_desc.units, 0, sizeof(pmUnits));
    DEBUG_LOG(
        "STATSD: adding metric[%lu] %s %s from %s\n",
        data->pcp_metric_count, name, pmIDStr(pmid), name
    );

    // now add the new pmid into pmdaTree
    pmdaTreeInsert(data->pcp_pmns, pmid, name);

    // debugging
    g_item_num += 1;
    data->pcp_metric_count += 1;
}

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
    }
    pmsprintf(name, 64, "statsd.pmda.received");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 0), name);
    pmsprintf(name, 64, "statsd.pmda.parsed");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 1), name);
    pmsprintf(name, 64, "statsd.pmda.dropped");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 2), name);
    pmsprintf(name, 64, "statsd.pmda.aggregated");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 3), name);
    pmsprintf(name, 64, "statsd.pmda.time_spent_parsing");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 4), name);
    pmsprintf(name, 64, "statsd.pmda.time_spent_aggregating");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 5), name);
    data->pcp_metric_count = 6;    
    iterate_over_metrics(data->metrics_storage, create_metric_iteration_callback, pmda);
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
                static char oneliner[] = "Total time in microseconds spent parsing metrics";
                static char full_description[] = 
                    "Total time in microseconds spent parsing metrics. Includes time spent parsing a datagram and failing midway.\n";
                *buffer = (type & PM_TEXT_ONELINE) ? oneliner : full_description;
                return 0;
            }
            case 5:
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
    // this could be some mechanism that resolves metric help text
    // return pmdaText(ident, type, buffer, pmda);
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
 * @arg name - 
 * @arg traverse -
 * @arg children - 
 * @arg status -
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

/**
 * This callback deals with one request unit which may be part of larger request of PDU_FETCH
 * @arg pmdaMetric - requested metric, along with user data, in out case PMDA extension structure (contains agent-specific private data)
 * @arg inst - requested metric instance
 * @arg atom - atom that should be populated with request response
 * @return value less then 0 signalizes error, equal to 0 means that metric is not available, greater then 0 is success
 */
int
statsd_fetch_callback(pmdaMetric* mdesc, unsigned int instance, pmAtomValue* atom) {
    struct pmda_data_extension* data = (struct pmda_data_extension*) mdesc->m_user;
    struct agent_config* config = data->config;
    struct pmda_stats_container* stats = data->stats_storage;
    struct pmda_metrics_container* metrics = data->metrics_storage;
    dict* reverse_lookup_table = data->pcp_metric_reverse_lookup;
    unsigned int cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int item = pmID_item(mdesc->m_desc.pmid);
    switch (cluster) {
        /* stats - info about agent itself */
        case 0:
            switch (item) {
                /* received */
                case 0:
                    atom->ull = get_agent_stat(config, stats, STAT_RECEIVED);
                    break;
                /* parsed */
                case 1:
                    atom->ull = get_agent_stat(config, stats, STAT_PARSED);
                    break;
                /* thrown away */
                case 2:
                    atom->ull = get_agent_stat(config, stats, STAT_DROPPED);
                    break;
                /* aggregated */
                case 3:
                    atom->ull = get_agent_stat(config, stats, STAT_AGGREGATED);
                    break;
                /* time_spent_parsing */
                case 4:
                    atom->ull = get_agent_stat(config, stats, STAT_TIME_SPENT_PARSING);
                    break;
                /* time_spent_aggregating */
                case 5:
                    atom->ull = get_agent_stat(config, stats, STAT_TIME_SPENT_AGGREGATING);
                    break;
                default:
                    return PM_ERR_PMID;
            }
            break;
        case 1:
        {
            const char* reverse_lookup_key = pmIDStr(mdesc->m_desc.pmid);
            dictEntry* entry = dictFind(reverse_lookup_table, reverse_lookup_key);
            if (entry == NULL) {
                return PM_ERR_PMID;
            }
            char* metric_name = ((struct pcp_reverse_lookup_record*)entry->v.val)->name;
            char* metric_hashtable_key = create_metric_dict_key(metric_name, NULL);
            struct metric* result;
            if (find_metric_by_name(metrics, metric_hashtable_key, &result)) {
                switch (result->type) {
                    case METRIC_TYPE_COUNTER:
                    case METRIC_TYPE_GAUGE:
                        atom->d = *(double*)result->value;
                        break;
                    case METRIC_TYPE_DURATION: {
                        struct duration_values_meta* meta =
                            (struct duration_values_meta*) malloc(sizeof(struct duration_values_meta));
                        get_duration_values_meta(data->config, result, &meta);
                        switch (instance) {
                            case 0:
                                atom->d = meta->min;
                                break;
                            case 1:
                                atom->d = meta->max;
                                break;
                            case 2:
                                atom->d = meta->median;
                                break;
                            case 3:
                                atom->d = meta->average;
                                break;
                            case 4:
                                atom->d = meta->percentile90;
                                break;
                            case 5:
                                atom->d = meta->percentile95;
                                break;
                            case 6:
                                atom->d = meta->percentile99;
                                break;
                            case 7:
                                atom->d = meta->count;
                                break;
                            case 8:
                                atom->d = meta->std_deviation;
                                break;
                            default:
                                free(meta);
                                return PM_ERR_INST;
                        }
                        free(meta);
                        break;
                    }
                    default:
                        return PM_ERR_PMID;
                }
            } else {
                return PM_ERR_PMID;
            }
            break;
        }
        default:
            return PM_ERR_PMID;
    }
    return PMDA_FETCH_STATIC;
}
