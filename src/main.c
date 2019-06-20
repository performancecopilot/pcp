#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <chan/chan.h>
#include <signal.h>

#include "config-reader.h"
#include "network-listener.h"
#include "aggregators.h"
#include "pcp.h"
#include "utils.h"

#if _TEST_TARGET == 0

void signal_handler(int num) {
    if (num == SIGUSR1) {
        aggregator_request_output();
    }
}

int main(int argc, char **argv)
{
    signal(SIGUSR1, signal_handler);

    pthread_t network_listener;
    pthread_t parser;
    pthread_t aggregator;
    pthread_t pcp;

    agent_config* config = (agent_config*) malloc(sizeof(agent_config));
    ALLOC_CHECK(NULL, "Unable to asssign memory for agent config.");
    int config_src_type = argc >= 2 ? READ_FROM_CMD : READ_FROM_FILE;
    config = read_agent_config(config_src_type, "statsd-pmda.ini", argc, argv);
    init_loggers(config);
    print_agent_config(config);

    chan_t* unprocessed_datagrams_q = chan_init(config->max_unprocessed_packets);
    if (unprocessed_datagrams_q == NULL) DIE("Unable to create channel network listener -> parser.");
    chan_t* parsed_datagrams_q = chan_init(config->max_unprocessed_packets);
    if (parsed_datagrams_q == NULL) DIE("Unable to create channel parser -> aggregator.");
    chan_t* aggregator_to_pcp = chan_init(0);
    if (aggregator_to_pcp == NULL) DIE("Unable to create channel aggregator -> pcp.");
    chan_t* pcp_to_aggregator = chan_init(0);    
    if (pcp_to_aggregator == NULL) DIE("Unable to create channel pcp -> aggregator.");

    metrics* m = init_metrics(config);
    network_listener_args* listener_args = create_listener_args(config, unprocessed_datagrams_q);
    parser_args* parser_args = create_parser_args(config, unprocessed_datagrams_q, parsed_datagrams_q);
    aggregator_args* aggregator_args = create_aggregator_args(config, parsed_datagrams_q, aggregator_to_pcp, pcp_to_aggregator, m);
    pcp_args* pcp_args = create_pcp_args(config, pcp_to_aggregator, aggregator_to_pcp);

    int pthread_errno = 0; 
    pthread_errno = pthread_create(&network_listener, NULL, network_listener_exec, listener_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&parser, NULL, parser_exec, parser_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&aggregator, NULL, aggregator_exec, aggregator_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&pcp, NULL, pcp_pmda_exec, pcp_args);
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

    chan_close(unprocessed_datagrams_q);
    chan_close(parsed_datagrams_q);
    chan_close(aggregator_to_pcp);
    chan_close(pcp_to_aggregator);
    chan_dispose(unprocessed_datagrams_q);
    chan_dispose(parsed_datagrams_q);
    chan_dispose(aggregator_to_pcp);
    chan_dispose(pcp_to_aggregator);
    return EXIT_SUCCESS;
}

#endif
