#ifndef _TEST_TARGET

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <chan/chan.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <pcp/pmapi.h>

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "aggregator-metrics.h"
#include "aggregator-stats.h"
#include "pmda-pcp-init.h"
#include "utils.h"

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

    struct agent_config* config = (struct agent_config*) malloc(sizeof(struct agent_config));
    ALLOC_CHECK(NULL, "Unable to asssign memory for agent config.");

    int sep = pmPathSeparator();
    char config_file_path[MAXPATHLEN];
    pmsprintf(config_file_path, MAXPATHLEN, "%s" "%c" "statsd" "%c" "statsd-pmda.ini", pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    config = read_agent_config(READ_FROM_FILE, config_file_path, argc, argv);
    if (config->debug) {
        print_agent_config(config);
    }

    struct pmda_metrics_container* metrics = init_pmda_metrics(config);
    struct pmda_stats_container* stats = init_pmda_stats(config);
    init_loggers(config);

    pmdaInterface* interface = init_pmda(config, metrics, stats, argc, argv);

    chan_t* network_listener_to_parser = chan_init(config->max_unprocessed_packets);
    if (network_listener_to_parser == NULL) DIE("Unable to create channel network listener -> parser.");
    chan_t* parser_to_aggregator = chan_init(config->max_unprocessed_packets);
    if (parser_to_aggregator == NULL) DIE("Unable to create channel parser -> aggregator.");

    struct network_listener_args* listener_args = create_listener_args(config, network_listener_to_parser);
    struct parser_args* parser_args = create_parser_args(config, network_listener_to_parser, parser_to_aggregator);
    struct aggregator_args* aggregator_args = create_aggregator_args(config, parser_to_aggregator, metrics, stats);

    int pthread_errno = 0; 
    pthread_errno = pthread_create(&network_listener, NULL, network_listener_exec, listener_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&parser, NULL, parser_exec, parser_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&aggregator, NULL, aggregator_exec, aggregator_args);
    PTHREAD_CHECK(pthread_errno);

    pmdaConnect(interface);
    pmdaMain(interface);

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
    return EXIT_SUCCESS;
}

#endif
