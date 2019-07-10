#include <chan/chan.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#include "pcp.h"
#include "utils.h"
#include "config-reader.h"

struct pmda_stats {
    long int received;
    long int parsed;
    long int thrown_away;
    long int aggregated;
    pthread_mutex_t received_lock;
    pthread_mutex_t parsed_lock;
    pthread_mutex_t thrown_away_lock;
    pthread_mutex_t aggregated_lock;
} pmda_stats;

struct pmda_data_extension {
    struct agent_config* config;
    chan_t* pcp_to_aggregator;
    chan_t* agregator_to_pcp;
    chan_t* stats_sink;
    char** argv;
    char* username;
    struct pmda_stats* stats;
    int argc;
    char helpfile_path[MAXPATHLEN];
    pmdaOptions opts;
    pmLongOptions longopts;
} pmda_data_extension;

struct pmda_stats_collector_args {
    struct pmda_data_extension* data_extension;
} pmda_stats_collector_args;

static pmdaMetric metrictab[] = {
    /* received */
    { 
        NULL, 
        {
            PMDA_PMID(0, 0),
            PM_TYPE_U32,
            PM_INDOM_NULL,
            PM_SEM_INSTANT,
            PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
        }
    },
    /* parsed */
    { 
        NULL, 
        {
            PMDA_PMID(0, 1),
            PM_TYPE_U32,
            PM_INDOM_NULL,
            PM_SEM_INSTANT,
            PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
        }
    },
    /* thrown away */
    { 
        NULL, 
        {
            PMDA_PMID(0, 2),
            PM_TYPE_U32,
            PM_INDOM_NULL,
            PM_SEM_INSTANT,
            PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
        }
    },
    /* aggregated */
    {
        NULL,
        {
            PMDA_PMID(0, 3),
            PM_TYPE_U32,
            PM_INDOM_NULL,
            PM_SEM_INSTANT,
            PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
        }
    }
};

/**
 * Create Stat message
 * @arg type - Type of stat message
 * @arg data - Arbitrary user data which kind is defined by the type of stat message
 * @return new stat message construct 
 */
struct stat_message*
create_stat_message(enum STAT_MESSAGE_TYPE type, void* data) {
    struct stat_message* stat = (struct stat_message*) malloc(sizeof(stat_message));
    ALLOC_CHECK("Unable to allocate memory for stat message.");
    stat->type = type;
    stat->data = data;
    return stat;
}

/**
 * Frees up stat message
 * @arg stat - Stat message to be freed
 */
void
free_stat_message(struct stat_message* stat) {
    if (stat != NULL) {
        // There will possibly be switch statement when data will be variable by type
        if (stat->data != NULL) {
            free(stat->data);
        }
        free(stat);
    }
}


// typedef struct {
//   char *name;                   /* strdup client name */
//   void *addr;                   /* mmap */
//   int vcnt;                     /* number of values */
//   int mcnt1;                    /* number of metrics */
//   int lcnt;                     /* number of labels */
//   int cluster;                  /* cluster identifier */
//   pid_t pid;                    /* process identifier */
//   __int64_t len;                /* mmap region len */
//   __uint64_t gen;               /* generation number on open */
// } stats_t;

// typedef struct {
//     pmdaMetric* metrics;
//     pmdaIndom* indoms;
//     pmdaNameSpace* pmns;
//     stats_t *slist;
//     int scnt;
//     int reload;
// } agent_t;

// #define MAX_STATSD_COUNT 10000 /* enforce reasonable limits */
// #define MAX_STATSD_CLUSTER ((1 << 12) - 1)

// /**
//  * - slightly edited source for more readability from pmdammv
//  * Check cluster number validity (must be in range 0 .. 1<<12).
//  */
// static int
// valid_cluster(int requested) {
//   return (requested >= 0 && requested <= MAX_STATSD_CLUSTER);
// }

// /**
//  * - slightly edited source for more readability from pmdammv
//  * Choose an unused cluster ID whole honouring specific requests.
//  * If a specific (non-zero) cluster is requested we always use it.
//  */
// static int choose_cluster(agent_t *ap, int requested, const char *path) {
//   int i;

//   if (!requested) {
//     int next_cluster = 1;

//     for (i = 0; i < ap->scnt; i++) {
//       if (ap->slist[i].cluster == next_cluster) {
//         next_cluster++;
//         i = 0; /* restart, we're filling holes */
//       }
//     }
//     if (!valid_cluster(next_cluster))
//       return -EAGAIN;

//     return next_cluster;
//   }

//   if (!valid_cluster(requested))
//     return -EINVAL;

//   for (i = 0; i < ap->scnt; i++) {
//     if (ap->slist[i].cluster == requested) {
//       if (pmDebugOptions.appl0)
//         pmNotifyErr(LOG_DEBUG, "MMV: %s: duplicate cluster %d in use",
//                     pmGetProgname(), requested);
//       break;
//     }
//   }
//   return requested;
// }

// /**
//  * Checks if we need to reload metric namespace. Possible causes:
//  * - yet unmapped metric received
//  * @arg pmda - PMDA extension structure (contains agent-specific private data)
//  */
// static void
// statsd_possible_reload(pmdaExt* pmda) {
//     (void)pmda;
// } 

/**
 * Wrapper around pmdaDesc, called before control is passed to pmdaDesc
 * @arg pm_id - Instance domain
 * @arg desc - Performance Metric Descriptor
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
// static int
// statsd_desc(pmID pm_id, pmDesc* desc, pmdaExt* pmda) {
//     statsd_possible_reload(pmda);
//     return pmdaDesc(pm_id, desc, pmda);
// }

/**
 * Wrapper around pmdaText, called before control is passed to pmdaText
 * @arg ident -
 * @arg type - Base data type
 * @arg buffer - 
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
// static int
// statsd_text(int ident, int type, char** buffer, pmdaExt* pmda) {
//     statsd_possible_reload(pmda);
//     return pmdaText(ident, type, buffer, pmda);
// }

/**
 * Wrapper around pmdaInstance, called before control is passed to pmdaInstance
 * @arg in_dom - Instance domain description
 * @arg inst -
 * @arg name -
 * @arg result -
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
// static int
// statsd_instance(pmInDom in_dom, int inst, char* name, pmInResult** result, pmdaExt* pmda) {
//     statsd_possible_reload(pmda);
//     return pmdaInstance(in_dom, inst, name, result, pmda);
// }

/**
 * Wrapper around pmdaFetch, called before control is passed to pmdaFetch
 * @arg num_pm_id -
 * @arg pm_id_list - Collection of instance domains
 * @arg resp -
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
// static int
// statsd_fetch(int num_pm_id, pmID pm_id_list[], pmResult** resp, pmdaExt* pmda) {
//     g_numfetch++;
//     statsd_possible_reload(pmda);
//     return pmdaFetch(num_pm_id, pm_id_list, resp, pmda);
// }

/**
 * Wrapper around pmdaStore, called before control is passed to pmdaStore
 * @arg result -
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
// static int
// statsd_store(pmResult* result, pmdaExt* pmda) {
//     statsd_possible_reload(pmda);
//     int i;
//     int j;
//     int value;
//     int status = 0;
//     pmValueSet* value_set = NULL;

//     for (i = 0; i < result->numpmid; i++) {
//         unsigned int cluster;
//         unsigned int item;
//         value_set = result->vset[i];
//         cluster = pmID_cluster(value_set->pmid);
//         item = pmID_item(value_set->pmid);

//         if (cluster == 0) { /* all storable metrics are cluster 0 */
//             switch (item) {
//                 case 0: /* statsd.numfetch */
//                     value = value_set->vlist[0].value.lval;
//                     if (value < 0) {
//                         status = PM_ERR_BADSTORE;
//                         value = 0;
//                     }
//                     g_numfetch = value;
//                     break;
//             }
//         } else {
//             status = PM_ERR_PMID;
//         }
//     }
//     return status;
// }

/**
 * Wrapper around pmdaTreePMID, called before control is passed to pmdaTreePMID
 * @arg name -
 * @arg pm_id - Instance domain
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
// static int
// statsd_pmid(const char* name, pmID* pm_id, pmdaExt* pmda) {
//     struct pmda_data_extension* config = (struct pmda_data_extension*)pmdaExtGetData(pmda);
//     statsd_possible_reload(pmda);
//     return pmdaTreePMID(config->pmns, name, pm_id);
// }

/**
 * Wrapper around pmdaTreeName, called before control is passed to pmdaTreeName
 * @arg pm_id - Instance domain
 * @arg nameset -
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
// static int
// statsd_name(pmID pm_id, char*** nameset, pmdaExt* pmda) {
//     struct pmda_data_extension* config = (struct pmda_data_extension*)pmdaExtGetData(pmda);
//     statsd_possible_reload(pmda);
//     return pmdaTreeName(config->pmns, pm_id, nameset);
// } 

/**
 * Wrapper around pmdaTreeChildren, called before control is passed to pmdaTreeChildren
 * @arg name - 
 * @arg traverse -
 * @arg children - 
 * @arg status -
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
// static int
// statsd_children(const char* name, int traverse, char*** children, int** status, pmdaExt* pmda) {
//     struct pmda_data_extension* config = (struct pmda_data_extension*)pmdaExtGetData(pmda);
//     statsd_possible_reload(pmda);
//     return pmdaTreeChildren(config->pmns, name, traverse, children, status);
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
    struct pmda_data_extension* config = (struct pmda_data_extension*)pmdaExtGetData(pmda);
    (void)config;
    int status = 0;

    status = pmdaLabel(ident, type, lp, pmda);
    return status;
}

/**
 * @arg in_dom - Instance domain description
 * @arg inst -
 * @arg lp - Provides name and value indexes in JSON string
 */
// static int
// statsd_label_callback(pmInDom in_dom, unsigned int inst, pmLabelSet** lp) {
//     return 0;
// }

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
    unsigned int cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int item = pmID_item(mdesc->m_desc.pmid);
    if (inst != PM_IN_NULL) {
        return PM_ERR_INST;
    }
    switch (cluster) {
        /* info about agent itself */
        case 0:
            switch (item) {
                /* received */
                case 0:
                    pthread_mutex_lock(&(data->stats->received_lock));
                    atom->l = data->stats->received;
                    pthread_mutex_unlock(&(data->stats->received_lock));
                    break;
                /* parsed */
                case 1:
                    pthread_mutex_lock(&(data->stats->parsed_lock));
                    atom->l = data->stats->parsed;
                    pthread_mutex_unlock(&(data->stats->parsed_lock));
                    break;
                case 2:
                /* thrown away */
                    pthread_mutex_lock(&(data->stats->thrown_away_lock));
                    atom->l = data->stats->thrown_away;
                    pthread_mutex_unlock(&(data->stats->thrown_away_lock));
                    break;
                /* aggregated */
                case 3:
                    pthread_mutex_lock(&(data->stats->aggregated_lock));
                    atom->l = data->stats->aggregated;
                    pthread_mutex_unlocked(&(data->stats->aggregated_lock));
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
 * Registers PMDA interface callbacks and wrappers.
 */
void
register_pmda_interface_v7_callbacks(pmdaInterface* dispatch) {
    // Wrappers (gates) called before control is handled to callbacks
    // Wrappers pass control to PCP pocedures which may be swapped out by custom callbacks
    // dispatch.version.seven.fetch = statsd_fetch;
	// dispatch.version.seven.store = statsd_store;
	// dispatch.version.seven.desc = statsd_desc;
	// dispatch.version.seven.text = statsd_text;
	// dispatch.version.seven.instance = statsd_instance;
	// dispatch.version.seven.pmid = statsd_pmid;
	// dispatch.version.seven.name = statsd_name;
	// dispatch.version.seven.children = statsd_children;
	// dispatch.version.seven.label = statsd_label;
    // Callbacks
	pmdaSetFetchCallBack(dispatch, statsd_fetch_callback);
    // pmdaSetLabelCallBack(dispatch, statsd_label_callback);
}

/**
 * Setup for Performance Metric Domain Agent for StatsD
 * - Sets up PMDA callbacks for handling PDU (Protocol Data Unit)
 */
void
init_statsd_pmda(pmdaInterface* dispatch, struct pmda_data_extension* data_ext) {
    int sep = pmPathSeparator();
    pmSetProgname(data_ext->argv[0]);
    pmGetUsername(data_ext->username);
    pmdaDaemon(
        dispatch,
        PMDA_INTERFACE_7,
        data_ext->argv[0],
        510,
        "statsd.log",
        data_ext->helpfile_path
    );
    pmdaGetOptions(
        data_ext->argc,
        data_ext->argv,
        data_ext->opts,
        dispatch
    );
    if (data_ext->opts.errors) {
        pmdaUsageMessage(data_ext->opts);
        pthread_exit(NULL);
    }
    if (data_ext->opts.username) {
        data_ext->username = data_ext->opts.username;
    }
    pmdaOpenLog(dispatch);
    pmdaSetData(dispatch, data_ext)
    pmSetProcessIdentity(data_ext->username);
    if (dispatch.status != 0) {
        pthread_exit(NULL);
    }
    /* registers callbacks */
    register_pmda_interface_v7_callbacks(dispatch);
    pmdaInit(dispatch, NULL, 0, metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
    pmdaConnect(dispatch);
    /* pmdaMain() returns only when unexpected error condition happens or where PMCD closes connection to PMDA. */
    pmdaMain(dispatch);
}

static void
init_statsd_pmda_stats(struct pmda_stats* stats) {
    stats = (struct pmda_stats*) malloc(sizeof(struct pmda_stats));
    ALLOC_CHECK("Unable to allocate memory for PMDA stats.");
    stats->received = 0;
    stats->parsed = 0;
    stats->thrown_away = 0;
    stats->aggregated = 0;
}

static void
init_statsd_pmda_data_ext(struct pmda_data_extension* data, struct pcp_args* args) {
    data = (struct pmda_data_extension*) malloc(sizeof(struct pmda_data_extension));
    ALLOC_CHECK("Unable to allocate memory for private PMDA procedures data.");
    data->config = args->config;
    data->pcp_to_aggregator = args->pcp_to_aggregator;
    data->aggregator_to_pcp = args->aggregator_to_pcp;
    data->argc = args->argc;
    data->argv = args->argv;
    pmGetUsername(data->username);
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
    pmsprintf(data->helpfile_path, sizeof(data->helpfile_path), "%s%c" "statsd" "%c" "help_statsd", pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    init_statsd_pmda_stats(data->stats);
}

static void*
process_stat_message(struct pmda_data_extension* data, struct stat_message* message) {
    struct pmda_stats* stats = data->stats;
    switch(message->type) {
        case STAT_RECEIVED_INC:
            pthread_mutex_lock(&stats->received_lock);
            stats->received += 1;
            pthread_mutex_unlock(&stats->received_lock);
            break;
        case STAT_PARSED_INC:
            pthread_mutex_lock(&stats->parsed_lock);
            stats->parsed += 1;
            pthread_mutex_unlock(&stats->parsed_lock);
            break;
        case STAT_THROWN_AWAY_INC:
            pthread_mutex_lock(&stats->thrown_away_lock);
            stats->thrown_away += 1;
            pthread_mutex_unlock(&stats->thrown_away_lock);
            break;
        case STAT_AGGREGATED_INC:
            pthread_mutex_lock(&stats->aggregated_lock);
            stats->aggregated = 0;
            pthread_mutex_unlock(&stats->aggregated_lock);
            break;
        case STAT_RECEIVED_RESET:
            pthread_mutex_lock(&stats->received_lock);
            stats->received = 0;
            pthread_mutex_unlock(&stats->received_lock);
            break;
        case STAT_PARSED_RESET:
            pthread_mutex_lock(&stats->parsed_lock);
            stats->parsed = 0;
            pthread_mutex_unlock(&stats->parsed_lock);
            break;
        case STAT_THROWN_AWAY_RESET:
            pthread_mutex_lock(&stats->thrown_away_lock);
            stats->thrown_away = 0;
            pthread_mutex_unlock(&stats->thrown_away_lock);
            break;
        case STAT_AGGREGATED_RESET:
            pthread_mutex_lock(&stats->aggregated_lock);
            stats->aggregated = 0;
            pthread_mutex_lock(&stats->aggregated_lock);
            break;
    }
}

/**
 * Main loop handling incoming PMDA stats 
 */
static void*
pmda_stats_collector_exec(void* args) {
    struct pmda_data_extension* data = ((struct pmda_stats_collector_args*)args)->data_extension;
    chan_t* stats_sink = data->stats_sink;
    struct stat_message* message;
    while(1) {
        switch(chan_select(&stats_sink, 1, (void*)&message, NULL, 0, NULL)) {
            case 0:
                process_stat_message(data, message);
                free_stat_message(message);
                break;
        }
    }
    pthread_exit(NULL);
}

/**
 * Main loop handling incoming responses from aggregators
 * @arg args - (pcp_args), see ~/src/pcp.h
 */
void*
pcp_pmda_exec(void* args) {
    struct pcp_args* args = (struct pcp_args*)args;

    // Create another thread that will handle incoming messages in stats_channel 
    // which serves as sink that handles all changes to StatsD PMDA related metrics (metric about agent itself) 
    pthread_t pmda_stats_collector;

    // Initialize
    pmdaInterface dispatch;
    struct pmda_data_extension* data_ext;
    init_statsd_pmda_data_ext(data_ext, args);
    init_statsd_pmda_interface(&dispatch, data_ext);

    struct pmda_stats_collector_args* listener_args = create_pmda_stats_collector_args(pmda_data_extension);
    int pthread_errno = 0;
    pthread_errno = pthread_create(&pmda_stats_collector, NULL, pmda_stats_collector_exec, listener_args);
    PTHREAD_CHECK(pthread_errno);

    /* init_statsd_pmda() returns only when unexpected error condition happens or where PMCD closes connection to PMDA. */
    init_statsd_pmda();

    if (pthread_join(pmda_stats_collector, NULL) != 0) {
        DIE("Error joining pcp listener thread.");
    }

    /* Exit pthread. */
    pthread_exit(NULL);
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
    chan_t* aggregator_request_channel,
    chan_t* aggregator_response_channel,
    chan_t* stats_sink;
    int argc,
    char** argv
) {
    struct pcp_args* pcp_args = (struct pcp_args*) malloc(sizeof(struct pcp_args));
    ALLOC_CHECK("Unable to assign memory for pcp thread arguments.");
    pcp_args->config = config;
    pcp_args->aggregator_request_channel = aggregator_request_channel;
    pcp_args->aggregator_response_channel = aggregator_response_channel;
    pcp_args->stats_sink = stats_sink;
    pcp_args->argc = argc;
    pcp_args->argv = argv;
    return pcp_args;
}

/**
 * Creates arguments for PMDA stats collector thread
 * @arg data_extension - Struture which contains all priv data that is shared in all PCP procedures callbacks, also includes agent config and channels
 * @return pmda_stats_collector_args
 */
static struct pmda_stats_collector_args*
create_pmda_stats_collector_args(pmda_data_extension* data_extension) {
    struct pmda_stats_collector_args* pmda_stats_collector_args = (struct pmda_stats_collector_args*) malloc(sizeof(struct pmda_stats_collector_args));
    ALLOC_CHECK("Unable to assign memory for pcp listener thread arguments.");
    pmda_stats_collector_args->data_extension = data_extension;
    return pmda_stats_collector_args;
}
