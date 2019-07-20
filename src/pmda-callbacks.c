#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "aggregator-metrics.h"
#include "aggregator-stats.h"
#include "utils.h"
#include "config-reader.h"
#include "pmda-pcp-init.h"
#include "pmda-callbacks.h"
#include "../domain.h"

/**
 * Checks if we need to reload metric namespace. Possible causes:
 * - yet unmapped metric received
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
static void
statsd_possible_reload(pmdaExt* pmda) {
    (void)pmda;
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
 * @arg ident -
 * @arg type - Base data type
 * @arg buffer - 
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
int
statsd_text(int ident, int type, char** buffer, pmdaExt* pmda) {
    statsd_possible_reload(pmda);
    return pmdaText(ident, type, buffer, pmda);
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
    statsd_possible_reload(pmda);
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
 * NOT IMPLEMENTED
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
 * NOT IMPLEMENTED
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
