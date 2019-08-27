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
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>

#include "config-reader.h"
#include "network-listener.h"
#include "parsers.h"
#include "aggregators.h"
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
    static char* network_end_message = "PMDASTATSD_EXIT";
    struct agent_config* config = ((struct parser_args*)args)->config;
    chan_t* network_listener_to_parser = ((struct parser_args*)args)->network_listener_to_parser;
    chan_t* parser_to_aggregator = ((struct parser_args*)args)->parser_to_aggregator;
    datagram_parse_callback parse_datagram;
    if ((int)config->parser_type == (int)PARSER_TYPE_BASIC) {
        parse_datagram = &basic_parser_parse;
    } else {
        parse_datagram = &ragel_parser_parse;
    }
    struct unprocessed_statsd_datagram* datagram;
    ALLOC_CHECK("Unable to allocate space for unprocessed statsd datagram.");
    char delim[] = "\n";
    struct timespec t0, t1;
    unsigned long time_spent_parsing;
    int should_exit;
    while(1) {
        should_exit = check_exit_flag();
        int success_recv = chan_recv(network_listener_to_parser, (void *)&datagram);
        if (success_recv == -1) {
            VERBOSE_LOG(2, "Error receiving message from network listener.");
            break;
        }
        if (strcmp(datagram->value, network_end_message) == 0) {
            VERBOSE_LOG(2, "Got network end message.");
            free_unprocessed_datagram(datagram);
            break;
        }
        if (should_exit) {
            VERBOSE_LOG(2, "Freeing datagrams after exit.");
            free_unprocessed_datagram(datagram);
            continue;
        }
        struct statsd_datagram* parsed;
        char* tok = strtok(datagram->value, delim);
        while (tok != NULL) {
            clock_gettime(CLOCK_MONOTONIC, &t0);
            int success = parse_datagram(tok, &parsed);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            struct parser_to_aggregator_message* message =
                (struct parser_to_aggregator_message*) malloc(sizeof(struct parser_to_aggregator_message));
            ALLOC_CHECK("Unable to assign memory for parser to aggregator message.");
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
        free_unprocessed_datagram(datagram);
    }
    VERBOSE_LOG(2, "Parser exiting.");
    struct parser_to_aggregator_message* message =
        (struct parser_to_aggregator_message*) malloc(sizeof(struct parser_to_aggregator_message));
    ALLOC_CHECK("Unable to assign memory for parser to aggregator message.");
    message->type = PARSER_RESULT_END;
    message->time = 0;
    message->data = NULL;
    chan_send(parser_to_aggregator, message);
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
 * Frees datagram
 */
void
free_datagram(struct statsd_datagram* datagram) {
    if (datagram != NULL) {
        if (datagram->name != NULL) {
            free(datagram->name);
        }
        if (datagram->tags != NULL) {
            free(datagram->tags);
        }
        free(datagram);
    }
}
