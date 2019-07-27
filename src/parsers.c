#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <chan/chan.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>

#include "config-reader.h"
#include "network-listener.h"
#include "parsers.h"
#include "parser-basic.h"
#include "parser-ragel.h"
#include "utils.h"

/**
 * Thread entrypoint - listens to incoming payload on a unprocessed channel
 * and sends over successfully parsed data over to Aggregator thread via processed channel
 * @arg args - parser_args
 */
void*
parser_exec(void* args) {
    pthread_setname_np(pthread_self(), "Parser");
    struct agent_config* config = ((struct parser_args*)args)->config;
    chan_t* network_listener_to_parser = ((struct parser_args*)args)->network_listener_to_parser;
    chan_t* parser_to_aggregator = ((struct parser_args*)args)->parser_to_aggregator;
    datagram_parse_callback parse_datagram;
    if ((int)config->parser_type == (int)PARSER_TYPE_BASIC) {
        parse_datagram = &basic_parser_parse;
    } else {
        parse_datagram = &ragel_parser_parse;
    }
    struct unprocessed_statsd_datagram* datagram =
        (struct unprocessed_statsd_datagram*) malloc(sizeof(struct unprocessed_statsd_datagram));
    ALLOC_CHECK("Unable to allocate space for unprocessed statsd datagram.");
    char delim[] = "\n";
    struct timespec t0, t1;
    unsigned long time_spent_parsing;
    while(1) {
        *datagram = (struct unprocessed_statsd_datagram) { 0 };
        chan_recv(network_listener_to_parser, (void *)&datagram);
        struct statsd_datagram* parsed;
        char* tok = strtok(datagram->value, delim);
        while (tok != NULL) {
            clock_gettime(CLOCK_MONOTONIC, &t0);
            int success = parse_datagram(tok, &parsed);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            struct parser_to_aggregator_message* message =
                (struct parser_to_aggregator_message*) malloc(sizeof(struct parser_to_aggregator_message));
            time_spent_parsing = (t1.tv_nsec) - (t0.tv_nsec);
            message->time = time_spent_parsing;
            if (success) {
                message->data = parsed;
                message->type = PARSER_RESULT_PARSED;
                chan_send(parser_to_aggregator, message);
            } else {
                message->data = NULL;
                message->type = PARSER_RESULT_DROPPED;
                chan_send(parser_to_aggregator, message);
            }
            tok = strtok(NULL, delim);
        }
    }
    pthread_exit(NULL);
}

/**
 * Creates arguments for parser thread
 * @arg config - Application config
 * @arg network_listener_to_parser - Network listener -> Parser
 * @arg parser_to_aggregator - Parser -> Aggregator
 * @return parser_args
 */
struct parser_args*
create_parser_args(struct agent_config* config, chan_t* network_listener_to_parser, chan_t* parser_to_aggregator) {
    struct parser_args* parser_args = (struct parser_args*) malloc(sizeof(struct parser_args));
    ALLOC_CHECK("Unable to assign memory for parser arguments.");
    parser_args->config = config;
    parser_args->network_listener_to_parser = network_listener_to_parser;
    parser_args->parser_to_aggregator = parser_to_aggregator;
    return parser_args;
}

/**
 * Prints out parsed datagram structure in human readable form.
 */
void
print_out_datagram(struct statsd_datagram* datagram) {
    printf("DATAGRAM: \n");
    printf("name: %s \n", datagram->name);
    printf("tags: %s \n", datagram->tags);
    printf("value: %lf \n", datagram->value);
    switch (datagram->type) {
        case METRIC_TYPE_COUNTER:
            printf("type: COUNTER \n");
            break;
        case METRIC_TYPE_GAUGE:
            printf("type: GAUGE \n");
            break;
        case METRIC_TYPE_DURATION:
            printf("type: DURATION \n");
            break;
        default:
            printf("type: null \n");
    }
    printf("sampling: %lf \n", datagram->sampling);
    printf("------------------------------ \n");
}

/**
 * Frees datagram
 */
void
free_datagram(struct statsd_datagram* datagram) {
    if (datagram->name != NULL) {
        free(datagram->name);
    }
    if (datagram->tags != NULL) {
        free(datagram->tags);
    }
    if (datagram != NULL) {
        free(datagram);
    }
}
