#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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
 * Flag to capture USR1 signal event, which is supposed mark request to output currently recorded data in debug file 
 */
static int g_output_requested = 0;
static pthread_mutex_t g_output_requested_lock;

/**
 * Thread startpoint - passes down given datagram to aggregator to record value it contains
 * @arg args - (aggregator_args), see ~/network-listener.h
 */
void*
aggregator_exec(void* args) {
    pthread_setname_np(pthread_self(), "Aggregator");
    struct agent_config* config = ((struct aggregator_args*)args)->config;
    struct pmda_metrics_container* metrics_container = ((struct aggregator_args*)args)->metrics_container;
    struct pmda_stats_container* stats_container = ((struct aggregator_args*)args)->stats_container;
    chan_t* parser_to_aggregator = ((struct aggregator_args*)args)->parser_to_aggregator;

    struct parser_to_aggregator_message* message;
    struct timespec t0, t1;
    unsigned long time_spent_aggregating;
    while(1) {
        switch(chan_select(&parser_to_aggregator, 1, (void *)&message, NULL, 0, NULL)) {
            case 0:
            {
                process_stat(config, stats_container, STAT_RECEIVED, NULL);
                switch (message->type) {
                    case PARSER_RESULT_PARSED:
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
                        break;
                    case PARSER_RESULT_DROPPED:
                        process_stat(config, stats_container, STAT_DROPPED, NULL);
                        process_stat(config, stats_container, STAT_TIME_SPENT_PARSING, &message->time);
                        break;
                }
                free_parser_to_aggregator_message(message);
            }
            break;
        }
        pthread_mutex_lock(&g_output_requested_lock);
        if (g_output_requested) {
            VERBOSE_LOG("Output of recorded values request caught.");
            write_metrics_to_file(config, metrics_container);
            write_stats_to_file(config, stats_container);
            VERBOSE_LOG("Recorded values output.");
            g_output_requested = 0;
        }
        pthread_mutex_unlock(&g_output_requested_lock);
    }
    pthread_exit(NULL);
}

/**
 * Sets flag notifying that output was requested
 */
void
aggregator_request_output() {
    pthread_mutex_lock(&g_output_requested_lock);
    g_output_requested = 1;
    pthread_mutex_unlock(&g_output_requested_lock);
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
