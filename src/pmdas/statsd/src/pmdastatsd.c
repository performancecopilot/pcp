/*
 * Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
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

void signal_handler(int num) {
    if (num == SIGUSR1) {
        aggregator_request_output();
    }
}


#define SET_INST_NAME(name, index) \
    instance[index].i_inst = index; \
    len = pmsprintf(buff, 20, "%s", name) + 1; \
    instance[index].i_name = (char*) malloc(sizeof(char) * len); \
    ALLOC_CHECK("Unable to allocate memory for static PMDA instance descriptor."); \
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
    ALLOC_CHECK("Unable to allocate memory for static PMDA instance domains.");
    
    pmdaInstid* instance;

    instance = (pmdaInstid*) malloc(sizeof(pmdaInstid) * 4);
    ALLOC_CHECK("Unable to allocate memory for static PMDA instance domain descriptor.");
    data->pcp_instance_domains[0].it_indom = STATS_METRIC_COUNTERS_INDOM;
    data->pcp_instance_domains[0].it_numinst = 4;
    data->pcp_instance_domains[0].it_set = instance;
    SET_INST_NAME("counter", 0);
    SET_INST_NAME("gauge", 1);
    SET_INST_NAME("duration", 2);
    SET_INST_NAME("total", 3);

    instance = (pmdaInstid*) malloc(sizeof(pmdaInstid) * 9);
    ALLOC_CHECK("Unable to allocate memory for static PMDA instance domain descriptors.");
    data->pcp_instance_domains[1].it_indom = STATSD_METRIC_DEFAULT_DURATION_INDOM;
    data->pcp_instance_domains[1].it_numinst = 9;
    data->pcp_instance_domains[1].it_set = instance;
    SET_INST_NAME("/min", 0);
    SET_INST_NAME("/max", 1);
    SET_INST_NAME("/median", 2);
    SET_INST_NAME("/average", 3);
    SET_INST_NAME("/percentile90", 4);
    SET_INST_NAME("/percentile95", 5);
    SET_INST_NAME("/percentile99", 6);
    SET_INST_NAME("/count", 7);
    SET_INST_NAME("/std_deviation", 8);

    instance = (pmdaInstid*) malloc(sizeof(pmdaInstid));
    ALLOC_CHECK("Unable to allocate memory for default dynamic metric instance domain descriptior");
    data->pcp_instance_domains[2].it_indom = STATSD_METRIC_DEFAULT_INDOM;
    data->pcp_instance_domains[2].it_numinst = 1;
    data->pcp_instance_domains[2].it_set = instance;
    SET_INST_NAME("/", 0);

    data->pcp_instance_domain_count = hardcoded_count;
}

/**
 * Registers hardcoded metrics before PMDA initializes itself fully
 * @arg pmda - PMDA extension structure (contains agent-specific private data)
 */
static void
create_statsd_hardcoded_metrics(struct pmda_data_extension* data) {
    size_t i;
    size_t hardcoded_count = 7;
    data->pcp_metrics = (pmdaMetric*) calloc(hardcoded_count, sizeof(pmdaMetric));
    ALLOC_CHECK("Unable to allocate space for static PMDA metrics.");
    // helper containing only reference to priv data same for all hardcoded metrics
    static struct pmda_metric_helper helper;
    helper.data = data;
    for (i = 0; i < hardcoded_count; i++) {
        data->pcp_metrics[i].m_user = &helper;
        data->pcp_metrics[i].m_desc.pmid = pmID_build(STATSD, 0, i);
        data->pcp_metrics[i].m_desc.type = PM_TYPE_U64;
        data->pcp_metrics[i].m_desc.sem = PM_SEM_INSTANT;
        if (i == 4) {
            data->pcp_metrics[i].m_desc.indom = STATS_METRIC_COUNTERS_INDOM;
        } else {
            data->pcp_metrics[i].m_desc.indom = PM_INDOM_NULL;
        }
        if (i == 5 || i == 6) {
            // time_spent_parsing / time_spent_aggregating
            data->pcp_metrics[i].m_desc.units.scaleTime = PM_TIME_NSEC;
            data->pcp_metrics[i].m_desc.units.scaleCount = 1;
        }
    }
    data->pcp_metric_count = hardcoded_count;
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

static int _isDSO = 1; /* for local contexts */
static pthread_t network_listener;
static pthread_t aggregator;
static pthread_t parser;
static chan_t* network_listener_to_parser;
static chan_t* parser_to_aggregator;
static struct agent_config config;

void
__PMDA_INIT_CALL
statsd_init(pmdaInterface *dispatch)
{
    struct pmda_metrics_container* metrics;
    struct pmda_stats_container* stats;
    struct pmda_data_extension data = { 0 };
    struct network_listener_args* listener_args;
    struct aggregator_args* aggregator_args;
    struct parser_args* parser_args;
    char config_file_path[MAXPATHLEN];
    char help_file_path[MAXPATHLEN];
    int pthread_errno, sep = pmPathSeparator();

    pmsprintf(
        config_file_path,
        MAXPATHLEN,
        "%s" "%c" "statsd" "%c" "pmdastatsd.ini",
        pmGetConfig("PCP_PMDAS_DIR"),
        sep, sep);

    if (_isDSO) {
        pmsprintf(
            help_file_path,
            MAXPATHLEN,
            "%s%c" "statsd" "%c" "help",
            pmGetConfig("PCP_PMDAS_DIR"),
            sep, sep);
	pmdaDSO(dispatch, PMDA_INTERFACE_7, "statsd DSO", help_file_path);
        read_agent_config(&config, dispatch, config_file_path, 0, NULL);
    } else {
        pmSetProcessIdentity(config.username);
    }

    signal(SIGUSR1, signal_handler);

    metrics = init_pmda_metrics(&config);
    stats = init_pmda_stats(&config);
    init_data_ext(&data, &config, metrics, stats);

    network_listener_to_parser = chan_init(config.max_unprocessed_packets);
    if (network_listener_to_parser == NULL)
	DIE("Unable to create channel network listener -> parser.");
    parser_to_aggregator = chan_init(config.max_unprocessed_packets);
    if (parser_to_aggregator == NULL)
	DIE("Unable to create channel parser -> aggregator.");

    listener_args = create_listener_args(&config, network_listener_to_parser);
    parser_args = create_parser_args(&config, network_listener_to_parser, parser_to_aggregator);
    aggregator_args = create_aggregator_args(&config, parser_to_aggregator, metrics, stats);

    pthread_errno = 0; 
    pthread_errno = pthread_create(&network_listener, NULL, network_listener_exec, listener_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&parser, NULL, parser_exec, parser_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&aggregator, NULL, aggregator_exec, aggregator_args);
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
stats_done(void)
{
    if (pthread_join(network_listener, NULL) != 0) {
        DIE("Error joining network network listener thread.");
    }
    if (pthread_join(parser, NULL) != 0) {
        DIE("Error joining datagram parser thread.");
    }
    if (pthread_join(aggregator, NULL) != 0) {
        DIE("Error joining datagram aggregator thread.");
    }

    chan_close(network_listener_to_parser);
    chan_close(parser_to_aggregator);
    chan_dispose(network_listener_to_parser);
    chan_dispose(parser_to_aggregator);
}

int
main(int argc, char** argv)
{
    int sep = pmPathSeparator();
    pmdaInterface dispatch = { 0 };
    char help_file_path[MAXPATHLEN];
    char config_file_path[MAXPATHLEN];

    _isDSO = 0;
    pmSetProgname(argv[0]);

    pmsprintf(
        config_file_path,
        MAXPATHLEN,
        "%s" "%c" "statsd" "%c" "pmdastatsd.ini",
        pmGetConfig("PCP_PMDAS_DIR"),
        sep, sep);

    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), STATSD, "statsd.log", help_file_path);

    read_agent_config(&config, &dispatch, config_file_path, argc, argv);
    init_loggers(&config);
    pmdaOpenLog(&dispatch);
    if (config.debug) {
        print_agent_config(&config);
    }
    if (config.show_version) {
        pmNotifyErr(LOG_INFO, "Version: %s", PCP_VERSION);
    }

    statsd_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    stats_done();

    return EXIT_SUCCESS;
}
