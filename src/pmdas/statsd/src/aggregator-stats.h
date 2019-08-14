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
#ifndef AGGREGATOR_STATS_
#define AGGREGATOR_STATS_

#include <stdio.h>
#include <pthread.h>

#include "config-reader.h"

enum STAT_TYPE {
    STAT_RECEIVED,
    STAT_PARSED,
    STAT_DROPPED,
    STAT_AGGREGATED,
    STAT_TIME_SPENT_PARSING,
    STAT_TIME_SPENT_AGGREGATING,
    STAT_TRACKED_METRIC
} STAT_TYPE;

struct metric_counters {
    size_t counter;
    size_t gauge;
    size_t duration;
} metric_counters;

struct pmda_stats {
    size_t received;
    size_t parsed;
    size_t dropped;
    size_t aggregated;
    size_t time_spent_parsing;
    size_t time_spent_aggregating;
    struct metric_counters* metrics_recorded;
} pmda_stats;

struct pmda_stats_container {
    struct pmda_stats* stats;
    pthread_mutex_t mutex;
} pmda_stats_container;

/**
 * Creates new pmda_stats_container structure, initializes all stats to 0
 */
struct pmda_stats_container*
init_pmda_stats(struct agent_config* config);

/**
 * Resets stat_message
 * @arg config
 * @arg s - Data structure shared with PCP thread containing all PMDA statistics data
 * @arg type - Type of message
 * 
 * Synchronized by mutex on pmda_stats_container
 */
void
reset_stat(struct agent_config* config, struct pmda_stats_container* s, enum STAT_TYPE type);

/**
 * Processes given stat_message
 * @arg config
 * @arg s - Data structure shared with PCP thread containing all PMDA statistics data
 * @arg type - Type of message
 * @arg data - Arbitrary message-related data
 * 
 * Synchronized by mutex on pmda_stats_container
 */
void
process_stat(struct agent_config* config, struct pmda_stats_container* s, enum STAT_TYPE type, void* data);

/**
 * Write PMDA stats
 * @arg config - config specifies where to write
 * @arg stats - Data structure shared with PCP thread containing all PMDA statistics data
 * 
 * Synchronized by mutex on pmda_stats_container
 */
void
write_stats_to_file(struct agent_config* config, struct pmda_stats_container* stats);

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
get_agent_stat(struct agent_config* config, struct pmda_stats_container* stats, enum STAT_TYPE type, void* data);

#endif
