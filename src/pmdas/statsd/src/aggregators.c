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
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <chan/chan.h>
#include <hdr/hdr_histogram.h>
#include <pcp/dict.h>
#include <pcp/pmapi.h>
#include <pthread.h>

#include "config-reader.h"
#include "parsers.h"
#include "network-listener.h"
#include "utils.h"
#include "parsers.h"
#include "aggregators.h"
#include "aggregator-metrics.h"
#include "aggregator-stats.h"

/**
 * Mutex guarding aggregator proccesing, so there are no race condiditions if we request debug output.
 */
static pthread_mutex_t g_aggregator_processing_lock;

/**
 * This is shared with a function thats called from signal handler, should debug data be requested
 */
static struct aggregator_args* g_aggregator_args = NULL;

/**
 * Thread startpoint - passes down given datagram to aggregator to record value it contains (should be used for a single new thread)
 * @arg args - aggregator_args
 */
void*
aggregator_exec(void* args) {
    pthread_setname_np(pthread_self(), "Aggregator");
    g_aggregator_args = (struct aggregator_args*)args;
    struct agent_config* config = ((struct aggregator_args*)args)->config;
    struct pmda_metrics_container* metrics_container = ((struct aggregator_args*)args)->metrics_container;
    struct pmda_stats_container* stats_container = ((struct aggregator_args*)args)->stats_container;
    chan_t* parser_to_aggregator = ((struct aggregator_args*)args)->parser_to_aggregator;

    struct parser_to_aggregator_message* message;
    struct timespec t0, t1;
    unsigned long time_spent_aggregating;
    int should_exit;
    while(1) {
        should_exit = check_exit_flag();
        int success_recv = chan_recv(parser_to_aggregator, (void*)&message);
        if (success_recv == -1) {
            VERBOSE_LOG(2, "Error received message from parser.");
            break;
        }
        if (message->type == PARSER_RESULT_END) {
            VERBOSE_LOG(2, "Got parser end message.");
            free_parser_to_aggregator_message(message);
            break;
        }
        if (should_exit) {
            free_parser_to_aggregator_message(message);
            continue;
        }
        pthread_mutex_lock(&g_aggregator_processing_lock);
        process_stat(config, stats_container, STAT_RECEIVED, NULL);
        if (message->type == PARSER_RESULT_PARSED) {
            clock_gettime(CLOCK_MONOTONIC, &t0);
            int status = process_metric(config, metrics_container, (struct statsd_datagram*) message->data);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            time_spent_aggregating = t1.tv_nsec - t0.tv_nsec;
            process_stat(config, stats_container, STAT_PARSED, NULL);
            process_stat(config, stats_container, STAT_TIME_SPENT_PARSING, &message->time);
            if (status) {
                process_stat(config, stats_container, STAT_AGGREGATED, NULL);
                process_stat(config, stats_container, STAT_TIME_SPENT_AGGREGATING, &time_spent_aggregating);
            } else {
                process_stat(config, stats_container, STAT_DROPPED, NULL);
            }
        } else if (message->type == PARSER_RESULT_DROPPED) {
            process_stat(config, stats_container, STAT_DROPPED, NULL);
            process_stat(config, stats_container, STAT_TIME_SPENT_PARSING, &message->time);
        }
        free_parser_to_aggregator_message(message);
        pthread_mutex_unlock(&g_aggregator_processing_lock);
    }
    VERBOSE_LOG(2, "Aggregator thread exiting.");
    pthread_exit(NULL);
}

/**
 * Outputs debug info
 */
void
aggregator_debug_output() {
    if (g_aggregator_args != NULL) {
        pthread_mutex_lock(&g_aggregator_processing_lock);
        write_metrics_to_file(g_aggregator_args->config, g_aggregator_args->metrics_container);
        write_stats_to_file(g_aggregator_args->config, g_aggregator_args->stats_container);
        pthread_mutex_unlock(&g_aggregator_processing_lock);
    }
}

/**
 * Frees pointer to aggregator message
 * @arg message - Message to be freed
 */
void
free_parser_to_aggregator_message(struct parser_to_aggregator_message* message) {
    if (message != NULL) {
        if (message->data != NULL) {
            free_datagram(message->data);
        }
        free(message);
    }
}

/**
 * Creates arguments for Agregator thread
 * @arg config - Application config
 * @arg parsed_channel - Parser -> Aggregator channel
 * @arg pcp_request_channel - PCP -> Aggregator channel
 * @arg pcp_response_channel - Aggregator -> PCP channel
 * @arg stats_sink - Channel for sending stats about PMDA itself
 * @return aggregator_args
 */
struct aggregator_args*
create_aggregator_args(
    struct agent_config* config,
    chan_t* parser_to_aggregator,
    struct pmda_metrics_container* m,
    struct pmda_stats_container* s
) {
    struct aggregator_args* aggregator_args = (struct aggregator_args*) malloc(sizeof(struct aggregator_args));
    ALLOC_CHECK("Unable to assign memory for parser aguments.");
    aggregator_args->config = config;
    aggregator_args->parser_to_aggregator = parser_to_aggregator;
    aggregator_args->metrics_container = m;
    aggregator_args->stats_container = s;
    return aggregator_args;
}
