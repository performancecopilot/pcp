
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <chan.h>

#include "config-reader/config-reader.h"
#include "statsd-parsers/statsd-parsers.h"
#include "consumers/consumers.h"
#include "utils/utils.h"



int main(int argc, char **argv)
{
    pthread_t network_listener;
    pthread_t datagram_parser;
    pthread_t datagram_consumer;

    agent_config* config = (agent_config*) malloc(sizeof(agent_config));
    ALLOC_CHECK(NULL, "Unable to asssign memory for agent config.");
    int config_src_type = argc >= 2 ? READ_FROM_CMD : READ_FROM_FILE;
    config = read_agent_config(config_src_type, "config", argc, argv);
    init_loggers(config);
    print_agent_config(config);

    chan_t* unprocessed_datagrams_q = chan_init(config->max_unprocessed_packets);
    chan_t* parsed_datagrams_q = chan_init(config->max_unprocessed_packets);

    statsd_listener_args* listener_args = create_listener_args(config, unprocessed_datagrams_q);
    statsd_parser_args* parser_args = create_parser_args(config, unprocessed_datagrams_q, parsed_datagrams_q);
    consumer_args* consumer_args = create_consumer_args(config, parsed_datagrams_q);

    int pthread_errno = 0; 
    pthread_errno = pthread_create(&network_listener, NULL, statsd_network_listen, listener_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&datagram_parser, NULL, statsd_parser_consume, parser_args);
    PTHREAD_CHECK(pthread_errno);
    pthread_errno = pthread_create(&datagram_consumer, NULL, consume_datagram, consumer_args);
    PTHREAD_CHECK(pthread_errno);

    if (pthread_join(network_listener, NULL) != 0) {
        die(__LINE__, "Error joining network listener thread.");
    }
    if (pthread_join(datagram_parser, NULL) != 0) {
        die(__LINE__, "Error joining datagram parser thread.");
    }
    if (pthread_join(datagram_consumer, NULL) != 0) {
        die(__LINE__, "Error joining datagram consumer thread.");
    }
    return 1;
}
