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
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "pmdastatsd.h"
#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metrics.h"
#include "aggregator-stats.h"
#include "pmda-callbacks.h"
#include "dict-callbacks.h"
#include "utils.h"
#include "domain.h"

#define VERSION 1

void signal_handler(int num) {
    if (num == SIGUSR1) {
        VERBOSE_LOG(2, "Handling SIGUSR1.");
        aggregator_debug_output();
    }
    if (num == SIGINT) {
        VERBOSE_LOG(2, "Handling SIGINT.");
        set_exit_flag();
    }
}

#define SET_INST_NAME(instance, name, index) \
    instance[index].i_inst = index; \
    len = pmsprintf(buff, 20, "%s", name) + 1; \
    instance[index].i_name = (char*) malloc(sizeof(char) * len); \
    ALLOC_CHECK(instance[index].i_name, "Unable to allocate memory for static PMDA instance descriptor."); \
    memcpy(instance[index].i_name, buff, len);

/**
 * Registers hardcoded instances before PMDA initializes itself fully
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
static void
create_statsd_hardcoded_instances(struct pmda_data_extension* data) {
    size_t len = 0;
    char buff[20];
    size_t hardcoded_count = 3;

    data->pcp_instance_domains = (pmdaIndom*) malloc(hardcoded_count * sizeof(pmdaIndom));
    ALLOC_CHECK(data->pcp_instance_domains, "Unable to allocate memory for static PMDA instance domains.");
    
    pmdaInstid* stats_metric_counters_indom = (pmdaInstid*) malloc(sizeof(pmdaInstid) * 4);
    ALLOC_CHECK(stats_metric_counters_indom, "Unable to allocate memory for static PMDA instance domain descriptor.");
    data->pcp_instance_domains[0].it_indom = STATS_METRIC_COUNTERS_INDOM;
    data->pcp_instance_domains[0].it_numinst = 4;
    data->pcp_instance_domains[0].it_set = stats_metric_counters_indom;
    SET_INST_NAME(stats_metric_counters_indom, "counter", 0);
    SET_INST_NAME(stats_metric_counters_indom, "gauge", 1);
    SET_INST_NAME(stats_metric_counters_indom, "duration", 2);
    SET_INST_NAME(stats_metric_counters_indom, "total", 3);

    pmdaInstid* statsd_metric_default_duration_indom = (pmdaInstid*) malloc(sizeof(pmdaInstid) * 9);
    ALLOC_CHECK(statsd_metric_default_duration_indom, "Unable to allocate memory for static PMDA instance domain descriptors.");
    data->pcp_instance_domains[1].it_indom = STATSD_METRIC_DEFAULT_DURATION_INDOM;
    data->pcp_instance_domains[1].it_numinst = 9;
    data->pcp_instance_domains[1].it_set = statsd_metric_default_duration_indom;
    
    SET_INST_NAME(statsd_metric_default_duration_indom, "/min", 0);
    SET_INST_NAME(statsd_metric_default_duration_indom, "/max", 1);
    SET_INST_NAME(statsd_metric_default_duration_indom, "/median", 2);
    SET_INST_NAME(statsd_metric_default_duration_indom, "/average", 3);
    SET_INST_NAME(statsd_metric_default_duration_indom, "/percentile90", 4);
    SET_INST_NAME(statsd_metric_default_duration_indom, "/percentile95", 5);
    SET_INST_NAME(statsd_metric_default_duration_indom, "/percentile99", 6);
    SET_INST_NAME(statsd_metric_default_duration_indom, "/count", 7);
    SET_INST_NAME(statsd_metric_default_duration_indom, "/std_deviation", 8);

    pmdaInstid* statsd_metric_default_indom = (pmdaInstid*) malloc(sizeof(pmdaInstid));
    ALLOC_CHECK(statsd_metric_default_indom, "Unable to allocate memory for default dynamic metric instance domain descriptor");
    data->pcp_instance_domains[2].it_indom = STATSD_METRIC_DEFAULT_INDOM;
    data->pcp_instance_domains[2].it_numinst = 1;
    data->pcp_instance_domains[2].it_set = statsd_metric_default_indom;
    SET_INST_NAME(statsd_metric_default_indom, "/", 0);

    data->pcp_instance_domain_count = hardcoded_count;
    data->pcp_hardcoded_instance_domain_count = hardcoded_count;
}

/**
 * Registers hardcoded metrics before PMDA initializes itself fully
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
static void
create_statsd_hardcoded_metrics(struct pmda_data_extension* data) {
    size_t i;
    size_t hardcoded_count = 14;
    data->pcp_metrics = (pmdaMetric*) malloc(hardcoded_count * sizeof(pmdaMetric));
    ALLOC_CHECK(data->pcp_metrics, "Unable to allocate space for static PMDA metrics.");
    // helper containing only reference to priv data same for all hardcoded metrics
    static struct pmda_metric_helper helper;
    size_t agent_stat_count = 7;
    helper.data = data;
    for (i = 0; i < hardcoded_count; i++) {
        data->pcp_metrics[i].m_user = &helper;
        data->pcp_metrics[i].m_desc.pmid = pmID_build(STATSD, 0, i);
        data->pcp_metrics[i].m_desc.sem = PM_SEM_INSTANT;
        if (i < agent_stat_count) {
            data->pcp_metrics[i].m_desc.type = PM_TYPE_U64;
            if (i == 4) {
                data->pcp_metrics[i].m_desc.indom = STATS_METRIC_COUNTERS_INDOM;
            } else {
                data->pcp_metrics[i].m_desc.indom = PM_INDOM_NULL;
            }            
        } else {
            if (i == 7) {
                data->pcp_metrics[i].m_desc.type = PM_TYPE_U64;
            } else if (i < 10 || i == 11) {
                data->pcp_metrics[i].m_desc.type = PM_TYPE_U32;
            } else {
                data->pcp_metrics[i].m_desc.type = PM_TYPE_STRING;
            }
            data->pcp_metrics[i].m_desc.indom = PM_INDOM_NULL;
        }
        if (i == 5 || i == 6) {
            // time_spent_parsing / time_spent_aggregating
            data->pcp_metrics[i].m_desc.units.pad = 0;
            data->pcp_metrics[i].m_desc.units.dimSpace = 0;
            data->pcp_metrics[i].m_desc.units.scaleCount = 0;
            data->pcp_metrics[i].m_desc.units.dimCount = 0;
            data->pcp_metrics[i].m_desc.units.scaleSpace = 0;
            data->pcp_metrics[i].m_desc.units.dimTime = 1;
            data->pcp_metrics[i].m_desc.units.scaleTime = PM_TIME_NSEC;
        } else {
            // rest
            memset(&data->pcp_metrics[i].m_desc.units, 0, sizeof(pmUnits));
        }
    }
    data->pcp_metric_count = hardcoded_count;
    data->pcp_hardcoded_metric_count = hardcoded_count;
}

static void
free_shared_data(struct agent_config* config, struct pmda_data_extension* data) {
    // frees config
    free(config->debug_output_filename);
    // remove metrics dictionary and related
    dictRelease(data->metrics_storage->metrics);
    // privdata will be left behind, need to remove manually
    free(data->metrics_storage->metrics_privdata);
    pthread_mutex_destroy(&data->metrics_storage->mutex);
    free(data->metrics_storage);
    // remove stats dictionary and related
    free(data->stats_storage->stats->metrics_recorded);
    free(data->stats_storage->stats);
    pthread_mutex_destroy(&data->stats_storage->mutex);
    free(data->stats_storage);
    // free instance map
    dictRelease(data->instance_map);
    // clear PCP metric table
    size_t i;
    for (i = 0; i < data->pcp_metric_count; i++) {
        size_t j = data->pcp_hardcoded_metric_count;
        if (!(i < j)) {
            free(data->pcp_metrics[i].m_user);
        }
    }
    free(data->pcp_metrics);    
    // clear not-hardcoded PCP instance domains
    for (i = 3; i < data->pcp_instance_domain_count; i++) {
        int j;
        // other instance domains may share certain instance names with 3 above, so be careful not to double free
        pmdaIndom domain = data->pcp_instance_domains[i];
        // if metric of type GAUGE/COUNTER shared instance names from STATSD_METRIC_DEFAULT_INDOM, its first instance name is '/'
        if (domain.it_set[0].i_name[1] == '\0') {
            for (j = 1; j < data->pcp_instance_domains[i].it_numinst; j++) {
                free(data->pcp_instance_domains[i].it_set[j].i_name);
            }
        }
        // if metric of type DURATION shared instance names from STATSD_METRIC_DEFAULT_DURATION_INDOM, its first instance name is '/min'
        else if (strcmp(domain.it_set[0].i_name, "/min") == 0) {
            for (j = 9; j < data->pcp_instance_domains[i].it_numinst; j++) {
                free(data->pcp_instance_domains[i].it_set[j].i_name);
            }
        }
        // else metric instance domain has no shared values
        else {
            for (j = 0; j < data->pcp_instance_domains[i].it_numinst; j++) {
                free(data->pcp_instance_domains[i].it_set[j].i_name);
            }
        }
        free(data->pcp_instance_domains[i].it_set);
    }
    // clear hardcoded PCP instance domains
    for (i = 0; i < 3; i++) {
        int j;
        for (j = 0; j < data->pcp_instance_domains[i].it_numinst; j++) {
            free(data->pcp_instance_domains[i].it_set[j].i_name);
        }
        free(data->pcp_instance_domains[i].it_set);
    }
    free(data->pcp_instance_domains);
    // Release PMNS tree
    pmdaTreeRelease(data->pcp_pmns);
}

/**
 * Initializes structure which is used as private data container across all PCP related callbacks
 * @arg args - All args passed to 'PCP exchange' thread
 */
static void
init_data_ext(
    struct pmda_data_extension* data,
    struct agent_config* config,
    struct pmda_metrics_container* metrics_storage,
    struct pmda_stats_container* stats_storage
) {
    static dictType instance_map_callbacks = {
        .hashFunction	= str_hash_callback,
        .keyCompare		= str_compare_callback,
        .keyDup		    = str_duplicate_callback,
        .keyDestructor	= str_hash_free_callback,
    };
    data->config = config;
    create_statsd_hardcoded_metrics(data);
    create_statsd_hardcoded_instances(data);
    data->metrics_storage = metrics_storage;
    data->stats_storage = stats_storage;
    data->instance_map = dictCreate(&instance_map_callbacks, NULL);
    data->generation = -1; // trigger first mapping of metrics for PMNS 
    data->notify = 0;
}

static void
main_PDU_loop(pmdaInterface* dispatch) {
    for(;;) {
        if (check_exit_flag()) break;
        if (__pmdaMainPDU(dispatch) < 0) break;
    }
    VERBOSE_LOG(2, "Exiting main PDU loop.");
}

static int _isDSO = 1; /* for local contexts */
static pthread_t network_listener;
static pthread_t aggregator;
static pthread_t parser;
static chan_t* network_listener_to_parser;
static chan_t* parser_to_aggregator;
static struct network_listener_args* listener_thread_args;
static struct aggregator_args* aggregator_thread_args;
static struct parser_args* parser_thread_args;
static struct agent_config config;
static struct pmda_data_extension data = { 0 };
char help_file_path[MAXPATHLEN];
char config_file_path[MAXPATHLEN];

void
__PMDA_INIT_CALL
statsd_init(pmdaInterface *dispatch)
{
    struct pmda_metrics_container* metricsp;
    struct pmda_stats_container* statsp;
    int pthread_errno, sep = pmPathSeparator();

    if (_isDSO) {
        pmsprintf(
            config_file_path,
            MAXPATHLEN,
            "%s" "%c" "statsd" "%c" "pmdastatsd.ini",
            pmGetConfig("PCP_PMDAS_DIR"),
            sep, sep);
	    pmdaDSO(dispatch, PMDA_INTERFACE_7, "statsd DSO", NULL);
        read_agent_config(&config, dispatch, config_file_path, 0, NULL);
    } else {
        pmSetProcessIdentity(config.username);
    }

    signal(SIGUSR1, signal_handler);

    metricsp = init_pmda_metrics(&config);
    statsp = init_pmda_stats(&config);
    init_data_ext(&data, &config, metricsp, statsp);

    network_listener_to_parser = chan_init(config.max_unprocessed_packets);
    if (network_listener_to_parser == NULL) {
	    DIE("Unable to create channel network listener -> parser.");
    }
    parser_to_aggregator = chan_init(config.max_unprocessed_packets);
    if (parser_to_aggregator == NULL) {
	    DIE("Unable to create channel parser -> aggregator.");
    }

    listener_thread_args = create_listener_args(&config, network_listener_to_parser);
    parser_thread_args = create_parser_args(&config, network_listener_to_parser, parser_to_aggregator);
    aggregator_thread_args = create_aggregator_args(&config, parser_to_aggregator, metricsp, statsp);

    pthread_errno = 0; 
    pthread_errno = pthread_create(&network_listener, NULL, network_listener_exec, listener_thread_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&parser, NULL, parser_exec, parser_thread_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&aggregator, NULL, aggregator_exec, aggregator_thread_args);
    PTHREAD_CHECK(pthread_errno);

    if (dispatch->status != 0) {
        pthread_exit(NULL);
    }
    dispatch->version.seven.fetch = statsd_fetch;
    dispatch->version.seven.desc = statsd_desc;
    dispatch->version.seven.text = statsd_text;
    dispatch->version.seven.instance = statsd_instance;
    dispatch->version.seven.pmid = statsd_pmid;
    dispatch->version.seven.name = statsd_name;
    dispatch->version.seven.children = statsd_children;
    dispatch->version.seven.label = statsd_label;
    // Callbacks
    pmdaSetFetchCallBack(dispatch, statsd_fetch_callback);
    pmdaSetLabelCallBack(dispatch, statsd_label_callback);

    pmdaSetData(dispatch, (void*) &data);
    pmdaSetFlags(dispatch, PMDA_EXT_FLAG_HASHED);
    pmdaInit(
        dispatch,
        data.pcp_instance_domains,
        data.pcp_instance_domain_count,
        data.pcp_metrics,
        data.pcp_metric_count
    );
}

static void
statsd_done(void) {    
    if (pthread_join(network_listener, NULL) != 0) {
        DIE("Error joining network network listener thread.");
    } else {
        VERBOSE_LOG(2, "Network listener thread joined.");
    }
    if (pthread_join(parser, NULL) != 0) {
        DIE("Error joining datagram parser thread.");
    } else {
        VERBOSE_LOG(2, "Parser thread joined.");
    }
    if (pthread_join(aggregator, NULL) != 0) {    
        DIE("Error joining datagram aggregator thread.");
    } else {
        VERBOSE_LOG(2, "Aggregator thread joined.");
    }

    free_shared_data(&config, &data);
    free(listener_thread_args);
    free(parser_thread_args);
    free(aggregator_thread_args);
    
    chan_close(network_listener_to_parser);
    chan_close(parser_to_aggregator);
    chan_dispose(network_listener_to_parser);
    chan_dispose(parser_to_aggregator);
}

int
main(int argc, char** argv)
{
    struct sigaction new_action, old_action;

    /* Set up the structure to specify the new action. */
    new_action.sa_handler = signal_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = SA_INTERRUPT;

    sigaction (SIGUSR1, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN) {
        sigaction (SIGUSR1, &new_action, NULL);
    }
    sigaction (SIGINT, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN) {
        sigaction (SIGINT, &new_action, NULL);
    }

    int sep = pmPathSeparator();
    pmdaInterface dispatch = { 0 };    

    _isDSO = 0;
    pmSetProgname(argv[0]);

    pmsprintf(
        config_file_path,
        MAXPATHLEN,
        "%s" "%c" "statsd" "%c" "pmdastatsd.ini",
        pmGetConfig("PCP_PMDAS_DIR"),
        sep, sep);

    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), STATSD, "statsd.log", NULL);

    read_agent_config(&config, &dispatch, config_file_path, argc, argv);
    init_loggers(&config);
    pmdaOpenLog(&dispatch);
    pmNotifyErr(LOG_INFO, "Config loaded from %s.", config_file_path);
    print_agent_config(&config);
    if (config.show_version) {
        pmNotifyErr(LOG_INFO, "Version: %d", VERSION);
    }

    statsd_init(&dispatch);
    pmdaConnect(&dispatch);
    main_PDU_loop(&dispatch);
    statsd_done();

    return EXIT_SUCCESS;
}
