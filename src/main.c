#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <chan/chan.h>
#include <signal.h>

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metrics.h"
#include "aggregator-stats.h"
#include "pcp.h"
#include "utils.h"

#if _TEST_TARGET == 0

void signal_handler(int num) {
    if (num == SIGUSR1) {
        aggregator_request_output();
    }
}

int
main(int argc, char** argv)
{
    signal(SIGUSR1, signal_handler);

    pthread_t network_listener;
    pthread_t parser;
    pthread_t aggregator;
    pthread_t pcp;    

    struct agent_config* config = (struct agent_config*) malloc(sizeof(struct agent_config));
    ALLOC_CHECK(NULL, "Unable to asssign memory for agent config.");
    int config_src_type = argc >= 2 ? READ_FROM_CMD : READ_FROM_FILE;
    config = read_agent_config(config_src_type, "statsd-pmda.ini", argc, argv);
    init_loggers(config);
    if (config->debug) {
        print_agent_config(config);
    }

    chan_t* network_listener_to_parser = chan_init(config->max_unprocessed_packets);
    if (network_listener_to_parser == NULL) DIE("Unable to create channel network listener -> parser.");
    chan_t* parser_to_aggregator = chan_init(config->max_unprocessed_packets);
    if (parser_to_aggregator == NULL) DIE("Unable to create channel parser -> aggregator.");
    
    struct pmda_metrics_container* m = init_pmda_metrics(config);
    struct pmda_stats_container* s = init_pmda_stats(config);

    struct network_listener_args* listener_args = create_listener_args(config, network_listener_to_parser);
    struct parser_args* parser_args = create_parser_args(config, network_listener_to_parser, parser_to_aggregator);
    struct aggregator_args* aggregator_args = create_aggregator_args(config, parser_to_aggregator, m, s);
    struct pcp_args* pmda_args = create_pcp_args(config, m, s, argc, argv);

    int pthread_errno = 0; 
    pthread_errno = pthread_create(&network_listener, NULL, network_listener_exec, listener_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&parser, NULL, parser_exec, parser_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&aggregator, NULL, aggregator_exec, aggregator_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&pcp, NULL, pcp_pmda_exec, pmda_args);
    PTHREAD_CHECK(pthread_errno);

    if (pthread_join(network_listener, NULL) != 0) {
        DIE("Error joining network network listener thread.");
    }
    if (pthread_join(parser, NULL) != 0) {
        DIE("Error joining datagram parser thread.");
    }
    if (pthread_join(aggregator, NULL) != 0) {
        DIE("Error joining datagram aggregator thread.");
    }
    if (pthread_join(pcp, NULL) != 0) {
        DIE("Error joinging pcp thread.");
    }

    chan_close(network_listener_to_parser);
    chan_close(parser_to_aggregator);
    chan_dispose(network_listener_to_parser);
    chan_dispose(parser_to_aggregator);
    return EXIT_SUCCESS;
}

#endif
