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
#include "utils.h"
#include "config-reader.h"
#include "pmda-callbacks.h"
#include "../domain.h"


static void
statsd_map_stats(pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*) pmdaExtGetData(pmda);
    int need_reload = 0;
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
    pmsprintf(name, sizeof(name), "statsd.pmda.received");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 0), name);
    pmsprintf(name, sizeof(name), "statsd.pmda.parsed");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 1), name);
    pmsprintf(name, sizeof(name), "statsd.pmda.dropped");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 2), name);
    pmsprintf(name, sizeof(name), "statsd.pmda.aggregated");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 3), name);
    pmsprintf(name, sizeof(name), "statsd.pmda.time_spent_parsing");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 4), name);
    pmsprintf(name, sizeof(name), "statsd.pmda.time_spent_aggregating");
    pmdaTreeInsert(data->pcp_pmns, pmID_build(pmda->e_domain, 0, 5), name);
    data->pcp_metric_count = 6;
    if (data->pcp_instance_domains != NULL) {
        size_t i;
        for (i = 0; i < data->pcp_instance_domain_count; i++) {
            free(data->pcp_instance_domains[i].it_set);
        }
        free(data->pcp_instance_domains);
        data->pcp_instance_domains = NULL;
        data->pcp_instance_domain_count = 0;
    }
    pmdaTreeRebuildHash(data->pcp_pmns, data->pcp_metric_count);
    data->reload = need_reload;    
}

/**
 * Checks if we need to reload metric namespace. Possible causes:
 * - yet unmapped metric received
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
static void
statsd_possible_reload(pmdaExt* pmda) {
    struct pmda_data_extension* data = (struct pmda_data_extension*) pmdaExtGetData(pmda);
    // if (need_reload) {
        if (pmDebugOptions.appl0) {
            DEBUG_LOG("statsd: %s: reloading", pmGetProgname());
        } else {
            VERBOSE_LOG("statsd: %s: reloading", pmGetProgname());
        }
        statsd_map_stats(pmda);
        pmda->e_indoms = data->pcp_instance_domains;
        pmda->e_nindoms = data->pcp_instance_domain_count;
        pmdaRehash(pmda, data->pcp_metrics, data->pcp_metric_count);
        if (pmDebugOptions.appl0) {
            DEBUG_LOG(
                "statsd: %s: %lu metrics and %lu instance domains after reload",
                pmGetProgname(),
                data->pcp_metric_count,
                data->pcp_instance_domain_count
            );
        }
    // }
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
    if (type & PM_TEXT_INDOM) {
        return PM_ERR_TEXT;
    }
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
    VERBOSE_LOG("statsd fetch");
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
 * NOT IMPLEMENTED
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
 * Wrapper around pmdaLabel, called before control is passed to pmdaLabel
 * @arg ident - 
 * @arg type - 
 * @arg lp - Provides name and value indexes in JSON string
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_label(int ident, int type, pmLabelSet** lp, pmdaExt* pmda) {
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
statsd_fetch_callback(pmdaMetric* mdesc, unsigned int inst, pmAtomValue* atom) {
    struct pmda_data_extension* data = (struct pmda_data_extension*) mdesc->m_user;
    struct agent_config* config = data->config;
    struct pmda_stats_container* stats = data->stats_storage;
    unsigned int cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int item = pmID_item(mdesc->m_desc.pmid);
    if (inst != PM_IN_NULL) {
        return PM_ERR_INST;
    }
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
        default:
            return PM_ERR_PMID;
    }
    return PMDA_FETCH_STATIC;
}
