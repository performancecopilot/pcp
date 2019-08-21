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
 * Used to signal pthread that its time to go home
 */
static int g_parser_exit = 0;

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
    struct unprocessed_statsd_datagram* datagram;
    ALLOC_CHECK("Unable to allocate space for unprocessed statsd datagram.");
    char delim[] = "\n";
    struct timespec t0, t1;
    unsigned long time_spent_parsing;
    int should_exit;
    int exit_loop = 0; 
    while(1 && !exit_loop) {
        should_exit = check_exit_flag();
        switch (chan_select(&network_listener_to_parser, 1, (void *)&datagram, NULL, 0, NULL)) {
            case 0:
            {
                if (should_exit) {
                    VERBOSE_LOG("FREEING DATAGRAMS after exit");
                    free_unprocessed_datagram(datagram);
                    break;
                }
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
                free_unprocessed_datagram(datagram);
                break;
            }
            default:
            {
                if (get_parser_exit()) {
                    exit_loop = 1;
                    break;
                }
            }
        }
    }
    VERBOSE_LOG("Parser exiting.");
    pthread_exit(NULL);
}

/**
 * Sets flag which is checked in main parser loop. 
 * If is true, parser loop stops sending messages trought channel and will free incoming messages.  
 */
void
set_parser_exit() {
    __sync_add_and_fetch(&g_parser_exit, 1);
}

/**
 * Gets exit flag 
 */
int
get_parser_exit() {
    return g_parser_exit;
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
    printf("------------------------------ \n");
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
