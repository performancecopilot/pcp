#ifndef AGGREGATOR_STATS_
#define AGGREGATOR_STATS_

#include <stdio.h>
#include <pthread.h>

#include "config-reader.h"

enum STAT_TYPE {
    STAT_RECEIVED,
    STAT_PARSED,
    STAT_THROWN_AWAY,
    STAT_AGGREGATED,
    STAT_TIME_SPENT_PARSING,
    STAT_TIME_SPENT_AGGREGATING,
} STAT_TYPE;

struct pmda_stats {
    unsigned long int received;
    unsigned long int parsed;
    unsigned long int thrown_away;
    unsigned long int aggregated;
    unsigned long int time_spent_parsing;
    unsigned long int time_spent_aggregating;    
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
 * Prints PMDA stats
 * @arg config
 * @arg f - Opened file handle, doesn't close it when finished
 * @arg stats - Data structure shared with PCP thread containing all PMDA statistics data
 * 
 * Synchronized by mutex on pmda_stats_container
 */
void
print_agent_stats(struct agent_config* config, FILE* f, struct pmda_stats_container* stats);

/**
 * Returns specified stat from pmda_stats_container
 * @arg config
 * @arg stats - Data structure shared with PCP thread containing all PMDA statistics data
 * @arg type - what stat to return
 * 
 * Synchronized by mutex on pmda_stats_container
 */
unsigned long int
get_agent_stat(struct agent_config* config, struct pmda_stats_container* stats, enum STAT_TYPE type);

#endif
