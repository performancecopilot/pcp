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

#include "pmda-pcp-init.h"
#include "pmda-callbacks.h"
#include "aggregator-metrics.h"
#include "aggregator-stats.h"
#include "utils.h"
#include "config-reader.h"
#include "../domain.h"

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

static pmdaOptions opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
};

/**
 * Initializes structure which is used as private data container accross all PCP related callbacks
 * @arg args - All args passed to 'PCP exchange' thread
 */
static struct pmda_data_extension*
create_statsd_pmda_data_ext(
    struct agent_config* config,
    struct pmda_metrics_container* metrics_storage,
    struct pmda_stats_container* stats_storage
) {
    struct pmda_data_extension* data = (struct pmda_data_extension*) malloc(sizeof(struct pmda_data_extension));
    ALLOC_CHECK("Unable to allocate memory for private PMDA procedures data.");
    data->config = config;
    data->total_metric_count = 0;
    data->metrics_storage = metrics_storage;
    data->stats_storage =stats_storage;
    pmGetUsername(&(data->username));
    int sep = pmPathSeparator();
    char cwd_path[MAXPATHLEN];
    getcwd(cwd_path, MAXPATHLEN);
    pmsprintf(
        data->helpfile_path,
        MAXPATHLEN,
        "%s%c" "statsd" "%c" "help",
        pmGetConfig("PCP_PMDAS_DIR"),
        sep, sep);
    return data;
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


pmdaInterface*
init_pmda(
    struct agent_config* config,
    struct pmda_metrics_container* metrics,
    struct pmda_stats_container* stats,
    int argc,
    char** argv
) {
    // pthread_setname_np(pthread_self(), "PCP exchange");

    // Initializes data extension which will serve as private data available accross all PCP callbacks
    struct pmda_data_extension* data = create_statsd_pmda_data_ext(config, metrics, stats);

    pmdaInterface* dispatch = (pmdaInterface*) malloc(sizeof(pmdaInterface));
    ALLOC_CHECK("Unable to allocate memory for pmda interface.");

    pmSetProgname(argv[0]);
    pmdaDaemon(dispatch, PMDA_INTERFACE_7, pmGetProgname(), STATSD, "statsd.log", data->helpfile_path);
    pmdaGetOptions(argc, argv, &opts, dispatch);
    if (opts.errors) {
        pmdaUsageMessage(&opts);
        exit(1);
    }
    if (opts.username) {
        data->username = opts.username;
    }
    pmdaOpenLog(dispatch);
    pmSetProcessIdentity(data->username);
    if (dispatch->status != 0) { 
        pthread_exit(NULL);
    }
    create_statsd_hardcoded_metrics(dispatch, data);;
    register_pmda_interface_v7_callbacks(dispatch);
    pmdaSetData(dispatch, (void*) data);
    pmdaSetFlags(dispatch, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dispatch, NULL, 0, data->pcp_metrics, data->hardcoded_metrics_count);

    return dispatch;
}
