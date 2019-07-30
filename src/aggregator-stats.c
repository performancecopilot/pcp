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
    *stats = (struct pmda_stats) { 0 };
    container->stats = stats;
    return container;
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
    switch(type) {
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
    }
    pthread_mutex_unlock(&s->mutex);
}

/**
 * Prints PMDA stats
 * @arg config
 * @arg f - Opened file handle, doesn't close it when finished
 * @arg stats - Data structure shared with PCP thread containing all PMDA statistics data
 * 
 * Synchronized by mutex on pmda_stats_container
 */
void
print_agent_stats(struct agent_config* config, FILE* f, struct pmda_stats_container* stats) {
    (void)config;
    pthread_mutex_lock(&stats->mutex);
    fprintf(f, "PMDA STATS: \n");
    fprintf(f, "received: %lu \n", stats->stats->received);
    fprintf(f, "parsed: %lu \n", stats->stats->parsed);
    fprintf(f, "thrown away: %lu \n", stats->stats->dropped);
    fprintf(f, "aggregated: %lu \n", stats->stats->aggregated);
    fprintf(f, "time spent parsing: %lu ns \n", stats->stats->time_spent_parsing);
    fprintf(f, "time spent aggregating: %lu ns \n", stats->stats->time_spent_aggregating);
    pthread_mutex_unlock(&stats->mutex);
}

/**
 * Returns specified stat from pmda_stats_container
 * @arg config
 * @arg stats - Data structure shared with PCP thread containing all PMDA statistics data
 * @arg type - what stat to return
 * 
 * Synchronized by mutex on pmda_stats_container
 */
unsigned long int
get_agent_stat(struct agent_config* config, struct pmda_stats_container* stats, enum STAT_TYPE type) {
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
        default:
            result = 0;
            break;
    }
    pthread_mutex_unlock(&stats->mutex);
    return result;
}
