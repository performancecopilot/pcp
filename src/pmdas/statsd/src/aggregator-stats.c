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
#include <pthread.h>

#include "aggregators.h"
#include "aggregator-stats.h"
#include "utils.h"

/**
 * Creates new pmda_stats_container structure, initializes all stats to 0
 */
struct pmda_stats_container*
init_pmda_stats(struct agent_config* config) {
    (void)config;
    struct pmda_stats_container* container =
        (struct pmda_stats_container*) malloc(sizeof(struct pmda_stats_container));
    ALLOC_CHECK("Unable to initialize container for PMDA stats.");
    pthread_mutex_init(&container->mutex, NULL);
    struct pmda_stats* stats = (struct pmda_stats*) malloc(sizeof(struct pmda_stats));
    ALLOC_CHECK("Unable to initialize PMDA stats.");
    struct metric_counters* counters = (struct metric_counters*) malloc(sizeof(struct metric_counters));
    ALLOC_CHECK("Unable to initialize metric counters stat structure.");
    *counters = (struct metric_counters) { 0 };
    *stats = (struct pmda_stats) { 0 };
    stats->metrics_recorded = counters;
    container->stats = stats;
    return container;
}

/**
 * Resets stat_message
 * @arg config
 * @arg s - Data structure shared with PCP thread containing all PMDA statistics data
 * @arg type - Type of message
 * 
 * Synchronized by mutex on pmda_stats_container
 */
void
reset_stat(struct agent_config* config, struct pmda_stats_container* s, enum STAT_TYPE type) {
    (void)config;
    pthread_mutex_lock(&s->mutex);
    switch (type) {
        case STAT_RECEIVED:
            s->stats->received = 0;
            break;
        case STAT_PARSED:
            s->stats->parsed = 0;
            break;
        case STAT_AGGREGATED:
            s->stats->aggregated = 0;
            break;
        case STAT_DROPPED:
            s->stats->dropped = 0;
            break;
        case STAT_TIME_SPENT_AGGREGATING:
            s->stats->time_spent_aggregating = 0;
            break;
        case STAT_TIME_SPENT_PARSING:
            s->stats->time_spent_parsing = 0;
            break;
        case STAT_TRACKED_METRIC:
            s->stats->metrics_recorded->counter = 0;
            s->stats->metrics_recorded->gauge = 0;
            s->stats->metrics_recorded->duration = 0;
            break;
    }
    pthread_mutex_unlock(&s->mutex);
}

/**
 * Processes given stat_message
 * @arg config
 * @arg s - Data structure shared with PCP thread containing all PMDA statistics data
 * @arg type - Type of message
 * @arg data - Arbitrary message-related data
 */
void
process_stat(struct agent_config* config, struct pmda_stats_container* s, enum STAT_TYPE type, void* data) {
    (void)config;
    pthread_mutex_lock(&s->mutex);
    switch (type) {
        case STAT_RECEIVED:
            s->stats->received += 1;
            break;
        case STAT_PARSED:
            s->stats->parsed += 1;
            break;
        case STAT_AGGREGATED:
            s->stats->aggregated += 1;
            break;
        case STAT_DROPPED:
            s->stats->dropped += 1;
            break;
        case STAT_TIME_SPENT_AGGREGATING:
            s->stats->time_spent_aggregating += *((long*) data);
            break;
        case STAT_TIME_SPENT_PARSING:
            s->stats->time_spent_parsing += *((long*) data);
            break;
        case STAT_TRACKED_METRIC:
        {
            enum METRIC_TYPE metric = (enum METRIC_TYPE)data;
            switch (metric) {
                case METRIC_TYPE_COUNTER:
                    s->stats->metrics_recorded->counter += 1;
                    break;
                case METRIC_TYPE_GAUGE:
                    s->stats->metrics_recorded->gauge += 1;
                    break;
                case METRIC_TYPE_DURATION:
                    s->stats->metrics_recorded->duration += 1;
                    break;
                case METRIC_TYPE_NONE:
                    break;
            }
            break;
        }
    }
    pthread_mutex_unlock(&s->mutex);
}

/**
 * Write PMDA stats
 * @arg config - config specifies where to write
 * @arg stats - Data structure shared with PCP thread containing all PMDA statistics data
 * 
 * Synchronized by mutex on pmda_stats_container
 */
void
write_stats_to_file(struct agent_config* config, struct pmda_stats_container* stats) {
    VERBOSE_LOG(0, "Writing stats to file...");
    pthread_mutex_lock(&stats->mutex);
    if (strlen(config->debug_output_filename) == 0) return; 
    int sep = pmPathSeparator();
    char debug_output[MAXPATHLEN];
    pmsprintf(
        debug_output,
        MAXPATHLEN,
        "%s" "%c" "pmcd" "%c" "statsd_%s",
        pmGetConfig("PCP_LOG_DIR"),
        sep, sep, config->debug_output_filename);
    FILE* f;
    f = fopen(debug_output, "a+");
    if (f == NULL) {
        pthread_mutex_unlock(&stats->mutex);
        VERBOSE_LOG(0, "Unable to open file for output.");
        return;
    }
    fprintf(f, "----------------\n");
    fprintf(f, "PMDA STATS: \n");
    fprintf(f, "received: %lu \n", stats->stats->received);
    fprintf(f, "parsed: %lu \n", stats->stats->parsed);
    fprintf(f, "thrown away: %lu \n", stats->stats->dropped);
    fprintf(f, "aggregated: %lu \n", stats->stats->aggregated);
    fprintf(f, "time spent parsing: %lu ns \n", stats->stats->time_spent_parsing);
    fprintf(f, "time spent aggregating: %lu ns \n", stats->stats->time_spent_aggregating);
    fprintf(
        f,
        "metrics tracked: counters: %lu, gauges: %lu, durations: %lu \n",
        stats->stats->metrics_recorded->counter,
        stats->stats->metrics_recorded->gauge,
        stats->stats->metrics_recorded->duration
    );
    fprintf(f, "-----------------\n");
    fclose(f);
    VERBOSE_LOG(0, "Wrote stats to debug file.");
    pthread_mutex_unlock(&stats->mutex);
}

/**
 * Returns specified stat from pmda_stats_container
 * @arg config
 * @arg stats - Data structure shared with PCP thread containing all PMDA statistics data
 * @arg type - what stat to return
 * @arg data - optional params for stat query
 * 
 * Synchronized by mutex on pmda_stats_container
 */
unsigned long int
get_agent_stat(struct agent_config* config, struct pmda_stats_container* stats, enum STAT_TYPE type, void* data) {
    (void)config;
    pthread_mutex_lock(&stats->mutex);
    long result;
    switch (type) {
        case STAT_RECEIVED:
            result = stats->stats->received;
            break;
        case STAT_PARSED:
            result = stats->stats->parsed;
            break;
        case STAT_DROPPED:
            result = stats->stats->dropped;
            break;
        case STAT_AGGREGATED:
            result = stats->stats->aggregated;
            break;
        case STAT_TIME_SPENT_PARSING:
            result = stats->stats->time_spent_parsing;
            break;
        case STAT_TIME_SPENT_AGGREGATING:
            result = stats->stats->time_spent_aggregating;
            break;
        case STAT_TRACKED_METRIC:
        {
            if (data != NULL) {
                enum METRIC_TYPE type = (enum METRIC_TYPE)data;
                if (type == METRIC_TYPE_COUNTER) {
                    result = stats->stats->metrics_recorded->counter;
                    break;
                }
                if (type == METRIC_TYPE_GAUGE) {
                    result = stats->stats->metrics_recorded->gauge;
                    break;
                }
                if (type == METRIC_TYPE_DURATION) {
                    result = stats->stats->metrics_recorded->duration;
                    break;
                }
            }
            size_t total = 0;
            total += stats->stats->metrics_recorded->counter;
            total += stats->stats->metrics_recorded->gauge;
            total += stats->stats->metrics_recorded->duration;
            result = total;
            break;
        }
        default:
            result = 0;
            break;
    }
    pthread_mutex_unlock(&stats->mutex);
    return result;
}
