#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "pcp.h"
#include "aggregator-metrics.h"
#include "aggregator-stats.h"
#include "utils.h"
#include "config-reader.h"
#include "../domain.h"

static struct pmda_data_extension* create_statsd_pmda_data_ext(struct pcp_args* args);
static void create_statsd_hardcoded_metrics(pmdaInterface* pi, struct pmda_data_extension* data);
static void statsd_possible_reload(pmdaExt* pmda);
static int statsd_desc(pmID pm_id, pmDesc* desc, pmdaExt* pmda);
static int statsd_text(int ident, int type, char** buffer, pmdaExt* pmda);
static int statsd_instance(pmInDom in_dom, int inst, char* name, pmInResult** result, pmdaExt* pmda);
static int statsd_fetch(int num_pm_id, pmID pm_id_list[], pmResult** resp, pmdaExt* pmda);
static int statsd_store(pmResult* result, pmdaExt* pmda);
static int statsd_label(int ident, int type, pmLabelSet** lp, pmdaExt* pmda);
static int statsd_label_callback(pmInDom in_dom, unsigned int inst, pmLabelSet** lp);
static int statsd_fetch_callback(pmdaMetric* mdesc, unsigned int inst, pmAtomValue* atom);
static void register_pmda_interface_v7_callbacks(pmdaInterface* dispatch);

/*
 * Set up the agent if running as a daemon.
 */
void*
pcp_pmda_exec(void* args)
{
    pthread_setname_np(pthread_self(), "PCP exchange");
    struct pcp_args* pcp_args = (struct pcp_args*)args;

    // Initializes data extension which will serve as private data available accross all PCP callbacks
    struct pmda_data_extension* data = create_statsd_pmda_data_ext(pcp_args);

    pmdaInterface dispatch;
    pmSetProgname(data->argv[0]);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), STATSD, "statsd.log", data->helpfile_path);
    pmdaGetOptions(data->argc, data->argv, &(data->opts), &dispatch);
    if (data->opts.errors) {
        pmdaUsageMessage(&(data->opts));
        exit(1);
    }
    if (data->opts.username) {
        data->username = data->opts.username;
    }
    pmdaOpenLog(&dispatch);
    pmSetProcessIdentity(data->username);
    if (dispatch.status != 0) { 
        pthread_exit(NULL);
    }
    create_statsd_hardcoded_metrics(&dispatch, data);;
    register_pmda_interface_v7_callbacks(&dispatch);
    pmdaSetData(&dispatch, (void*) data);
    pmdaSetFlags(&dispatch, PMDA_EXT_FLAG_HASHED);
    pmdaInit(&dispatch, NULL, 0, data->pcp_metrics, data->hardcoded_metrics_count);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    /* Exit pthread. */
    pthread_exit(NULL);
}

/**
 * Initializes structure which is used as private data container accross all PCP related callbacks
 * @arg args - All args passed to 'PCP exchange' thread
 */
static struct pmda_data_extension*
create_statsd_pmda_data_ext(struct pcp_args* args) {
    struct pmda_data_extension* data = (struct pmda_data_extension*) malloc(sizeof(struct pmda_data_extension));
    ALLOC_CHECK("Unable to allocate memory for private PMDA procedures data.");
    data->config = args->config;
    data->argc = args->argc;
    data->argv = args->argv;
    data->total_metric_count = 0;
    data->metrics_container = args->metrics_container;
    data->stats_container = args->stats_container;
    pmGetUsername(&(data->username));
    pmLongOptions long_opts[] = {
        PMDA_OPTIONS_HEADER("Options"),
        PMOPT_DEBUG,
        PMDAOPT_DOMAIN,
        PMDAOPT_LOGFILE,
        PMDAOPT_USERNAME,
        PMOPT_HELP,
        PMDA_OPTIONS_END
    };
    pmdaOptions opts = {
        .short_options = "D:d:l:U:?",
        .long_options = long_opts,
    };
    data->opts = opts;
    int sep = pmPathSeparator();
    pmsprintf(
        data->helpfile_path,
        MAXPATHLEN,
        "%s%c" "statsd" "%c" "help",
        pmGetConfig("PCP_PMDAS_DIR"),
        sep, sep);
    return data;
}

/**
 * NOT IMPLEMENTED
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
static int
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
static int
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
static int
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
static int
statsd_fetch(int num_pm_id, pmID pm_id_list[], pmResult** resp, pmdaExt* pmda) {
    statsd_possible_reload(pmda);
    return pmdaFetch(num_pm_id, pm_id_list, resp, pmda);
}

/**
 * Wrapper around pmdaStore, called before control is passed to pmdaStore
 * @arg result - Result to me populated
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
static int
statsd_store(pmResult* result, pmdaExt* pmda) {
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
// static int
// statsd_pmid(const char* name, pmID* pm_id, pmdaExt* pmda) {
//     struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData(pmda);
//     statsd_possible_reload(pmda);
//     return pmdaTreePMID(data->pmns, name, pm_id);
// }

/**
 * NOT IMPLEMENTED
 * Wrapper around pmdaTreeName, called before control is passed to pmdaTreeName
 * @arg pm_id - Instance domain
 * @arg nameset -
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
// static int
// statsd_name(pmID pm_id, char*** nameset, pmdaExt* pmda) {
//     struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData(pmda);
//     statsd_possible_reload(pmda);
//     return pmdaTreeName(data->pmns, pm_id, nameset);
// } 

/**
 * NOT IMPLEMENTED
 * Wrapper around pmdaTreeChildren, called before control is passed to pmdaTreeChildren
 * @arg name - 
 * @arg traverse -
 * @arg children - 
 * @arg status -
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
// static int
// statsd_children(const char* name, int traverse, char*** children, int** status, pmdaExt* pmda) {
//     struct pmda_data_extension* data = (struct pmda_data_extension*)pmdaExtGetData(pmda);
//     statsd_possible_reload(pmda);
//     return pmdaTreeChildren(data->pmns, name, traverse, children, status);
// }

/**
 * Wrapper around pmdaLabel, called before control is passed to pmdaLabel
 * @arg ident - 
 * @arg type - 
 * @arg lp - Provides name and value indexes in JSON string
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
static int
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
static int
statsd_label_callback(pmInDom in_dom, unsigned int inst, pmLabelSet** lp) {
    return 0;
}

/**
 * This callback deals with one request unit which may be part of larger request of PDU_FETCH
 * @arg pmdaMetric - requested metric, along with user data, in out case PMDA extension structure (contains agent-specific private data)
 * @arg inst - requested metric instance
 * @arg atom - atom that should be populated with request response
 * @return value less then 0 signalizes error, equal to 0 means that metric is not available, greater then 0 is success
 */
static int
statsd_fetch_callback(pmdaMetric* mdesc, unsigned int inst, pmAtomValue* atom) {
    struct pmda_data_extension* data = (struct pmda_data_extension*) mdesc->m_user;
    struct agent_config* config = data->config;
    struct pmda_stats_container* stats = data->stats_container;
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
                    atom->ull = get_agent_stat(config, stats, STAT_THROWN_AWAY);
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

/**
 * Registers hardcoded metrics that are registered before PMDA agent initializes itself fully
 * @arg pi - pmdaInterface carries all PCP stuff
 * @arg data - 
 */
static void
create_statsd_hardcoded_metrics(pmdaInterface* pi, struct pmda_data_extension* data) {
    data->hardcoded_metrics_count = 6;
    data->pcp_metrics = malloc(data->hardcoded_metrics_count * sizeof(pmdaMetric));
    ALLOC_CHECK("Unable to allocate space for static PMDA metrics.");
    size_t i;
    size_t max = data->hardcoded_metrics_count;
    for (i = 0; i < max; i++) {
        data->pcp_metrics[i].m_user = data;
        data->pcp_metrics[i].m_desc.pmid = pmID_build(pi->domain, 0, i);
        data->pcp_metrics[i].m_desc.type = PM_TYPE_U64;
        data->pcp_metrics[i].m_desc.indom = PM_INDOM_NULL;
        data->pcp_metrics[i].m_desc.sem = PM_SEM_INSTANT;
        if (i < 4) {
            memset(&data->pcp_metrics[i].m_desc.units, 0, sizeof(pmUnits));
        } else {
            // time_spent_parsing / time_spent_aggregating
            data->pcp_metrics[i].m_desc.units.dimSpace = 0;
            data->pcp_metrics[i].m_desc.units.dimTime = 0;
            data->pcp_metrics[i].m_desc.units.dimCount = 0;
            data->pcp_metrics[i].m_desc.units.pad = 0;
            data->pcp_metrics[i].m_desc.units.scaleSpace = 0;
            data->pcp_metrics[i].m_desc.units.scaleTime = PM_TIME_NSEC;
            data->pcp_metrics[i].m_desc.units.scaleCount = 1;
        }
        data->total_metric_count++;
    }

}

/**
 * Registers PMDA interface callbacks and wrappers.
 */
static void
register_pmda_interface_v7_callbacks(pmdaInterface* dispatch) {
    // Wrappers (gates) called before control is handled to callbacks
    // Wrappers pass control to PCP pocedures which may be swapped out by custom callbacks
    dispatch->version.seven.fetch = statsd_fetch;
	dispatch->version.seven.store = statsd_store;
	dispatch->version.seven.desc = statsd_desc;
	dispatch->version.seven.text = statsd_text;
	dispatch->version.seven.instance = statsd_instance;
	// dispatch.version.seven.pmid = statsd_pmid;
	// dispatch.version.seven.name = statsd_name;
	// dispatch.version.seven.children = statsd_children;
	dispatch->version.seven.label = statsd_label;
    // Callbacks
	pmdaSetFetchCallBack(dispatch, statsd_fetch_callback);
    pmdaSetLabelCallBack(dispatch, statsd_label_callback);
}

/**
 * Creates arguments for PCP thread
 * @arg config - Application config
 * @arg aggregator_request_channel - Aggregator -> PCP channel
 * @arg aggregator_response_channel - PCP -> Aggregator channel
 * @return pcp_args
 */
struct pcp_args*
create_pcp_args(
    struct agent_config* config,
    struct pmda_metrics_container* metrics_container,
    struct pmda_stats_container* stats_container,
    int argc,
    char** argv
) {
    struct pcp_args* args = (struct pcp_args*) malloc(sizeof(struct pcp_args));
    *args = (struct pcp_args) { 0 };
    ALLOC_CHECK("Unable to assign memory for pcp thread arguments.");
    args->config = config;
    args->metrics_container = metrics_container;
    args->stats_container = stats_container;
    args->argc = argc;
    args->argv = argv;
    return args;
}

