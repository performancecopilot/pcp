
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <chan/chan.h>
#include <signal.h>

#include "config-reader/config-reader.h"
#include "statsd-parsers/statsd-parsers.h"
#include "consumers/consumers.h"
#include "utils/utils.h"

void signal_handler(int num) {
    if (num == SIGUSR1) {
        consumer_request_output();
    }
}

int main(int argc, char **argv)
{
    signal(SIGUSR1, signal_handler);

    pthread_t network_listener;
    pthread_t datagram_parser;
    pthread_t datagram_consumer;

    agent_config* config = (agent_config*) malloc(sizeof(agent_config));
    ALLOC_CHECK(NULL, "Unable to asssign memory for agent config.");
    int config_src_type = argc >= 2 ? READ_FROM_CMD : READ_FROM_FILE;
    config = read_agent_config(config_src_type, "config.ini", argc, argv);
    init_loggers(config);
    print_agent_config(config);

    chan_t* unprocessed_datagrams_q = chan_init(config->max_unprocessed_packets);
    chan_t* parsed_datagrams_q = chan_init(config->max_unprocessed_packets);

    metrics* m = init_metrics(config);
    statsd_listener_args* listener_args = create_listener_args(config, unprocessed_datagrams_q);
    statsd_parser_args* parser_args = create_parser_args(config, unprocessed_datagrams_q, parsed_datagrams_q);
    consumer_args* consumer_args = create_consumer_args(config, parsed_datagrams_q, m);

    int pthread_errno = 0; 
    pthread_errno = pthread_create(&network_listener, NULL, statsd_network_listen, listener_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&datagram_parser, NULL, statsd_parser_consume, parser_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&datagram_consumer, NULL, consume_datagram, consumer_args);
    PTHREAD_CHECK(pthread_errno);

    if (pthread_join(network_listener, NULL) != 0) {
        DIE("Error joining network listener thread.");
    }
    if (pthread_join(datagram_parser, NULL) != 0) {
        DIE("Error joining datagram parser thread.");
    }
    if (pthread_join(datagram_consumer, NULL) != 0) {
        DIE("Error joining datagram consumer thread.");
    }

    chan_close(unprocessed_datagrams_q);
    chan_close(parsed_datagrams_q);
    chan_dispose(unprocessed_datagrams_q);
    chan_dispose(parsed_datagrams_q);
    return 1;
}
