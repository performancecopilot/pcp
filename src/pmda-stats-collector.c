#include <chan/chan.h>
#include <pthread.h>

#include "pmda-stats-collector.h"
#include "pcp.h"
#include "utils.h"

static void
process_stat_message(struct pmda_data_extension* data, struct stat_message* message);

/**
 * Main loop handling incoming PMDA stats
 * @arg args - pmda_stats_collector_args
 */
void*
pmda_stats_collector_exec(void* args) {
    pthread_setname_np(pthread_self(), "Stats");
    struct pmda_data_extension* data = ((struct pmda_stats_collector_args*)args)->data_extension;
    chan_t* stats_sink = data->stats_sink;
    struct stat_message* message;
    while(1) {
        switch(chan_select(&stats_sink, 1, (void*)&message, NULL, 0, NULL)) {
            case 0:
                process_stat_message(data, message);
                free_stat_message(message);
                break;
        }
    }
    pthread_exit(NULL);
}

/**
 * Processes givem stat_message, by modifying pmda_data_extension that is shared on all PCP related
 * callbacks in a STAT_MESSAGE_TYPE way
 * @arg data - PCP callbacks shared data
 * @arg message - Message to process
 */
static void
process_stat_message(struct pmda_data_extension* data, struct stat_message* message) {
    struct pmda_stats* stats = data->stats;
    switch(message->type) {
        case STAT_RECEIVED_INC:
            verbose_log("Received PMDA stat message: STAT_RECEIVED_INC.");
            pthread_mutex_lock(&(stats->received_lock));
            stats->received += 1;
            pthread_mutex_unlock(&(stats->received_lock));
            break;
        case STAT_PARSED_INC:
            verbose_log("Received PMDA stat message: STAT_PARSED_INC.");
            pthread_mutex_lock(&(stats->parsed_lock));
            stats->parsed += 1;
            pthread_mutex_unlock(&(stats->parsed_lock));
            break;
        case STAT_THROWN_AWAY_INC:
            verbose_log("Received PMDA stat message: STAT_THROWN_AWAY_INC.");
            pthread_mutex_lock(&(stats->thrown_away_lock));
            stats->thrown_away += 1;
            pthread_mutex_unlock(&(stats->thrown_away_lock));
            break;
        case STAT_AGGREGATED_INC:
            verbose_log("Received PMDA stat message: STAT_AGGREGATED_INC.");
            pthread_mutex_lock(&(stats->aggregated_lock));
            stats->aggregated += 1;
            pthread_mutex_unlock(&(stats->aggregated_lock));
            break;
        case STAT_RECEIVED_SET:
            verbose_log("Received PMDA stat message: STAT_RECEIVED_SET.");
            pthread_mutex_lock(&(stats->received_lock));
            stats->received = (unsigned long int) message->data;
            pthread_mutex_unlock(&(stats->received_lock));
            break;
        case STAT_PARSED_SET:
            verbose_log("Received PMDA stat message: STAT_PARSED_SET.");
            pthread_mutex_lock(&(stats->parsed_lock));
            stats->parsed = (unsigned long int) message->data;
            pthread_mutex_unlock(&(stats->parsed_lock));
            break;
        case STAT_THROWN_AWAY_SET:
            verbose_log("Received PMDA stat message: STAT_THROWN_AWAY_SET.");
            pthread_mutex_lock(&(stats->thrown_away_lock));
            stats->thrown_away = (unsigned long int) message->data;
            pthread_mutex_unlock(&(stats->thrown_away_lock));
            break;
        case STAT_AGGREGATED_SET:
            verbose_log("Received PMDA stat message: STAT_AGGREGATED_SET.");
            pthread_mutex_lock(&(stats->aggregated_lock));
            stats->aggregated = (unsigned long int) message->data;
            pthread_mutex_lock(&(stats->aggregated_lock));
            break;
        case STAT_TIME_SPENT_PARSING_ADD:
            verbose_log("Received PMDA stat message: STAT_TIME_SPENT_PARSING_ADD.");
            pthread_mutex_lock(&(stats->time_spent_parsing_lock));
            stats->time_spent_parsing += (unsigned long int) message->data;
            pthread_mutex_unlock(&(stats->time_spent_parsing_lock));
            break;
        case STAT_TIME_SPENT_AGGREGATING_ADD:
            verbose_log("Received PMDA stat message: STAT_TIME_SPENT_AGGREGATING_ADD.");
            pthread_mutex_lock(&(stats->time_spent_aggregating_lock));
            stats->time_spent_aggregating += (unsigned long int) message->data;
            pthread_mutex_unlock(&(stats->time_spent_aggregating_lock));
            break;
    }
}

/**
 * Creates Stat message
 * @arg type - Type of stat message
 * @arg data - Arbitrary user data which kind is defined by the type of stat message
 * @return new stat message construct 
 */
struct stat_message*
create_stat_message(enum STAT_MESSAGE_TYPE type, void* data) {
    struct stat_message* stat = (struct stat_message*) malloc(sizeof(stat_message));
    ALLOC_CHECK("Unable to allocate memory for stat message.");
    stat->type = type;
    stat->data = data;
    return stat;
}

/**
 * Frees up stat message
 * @arg stat - Stat message to be freed
 */
void
free_stat_message(struct stat_message* stat) {
    if (stat != NULL) {
        switch (stat->type) {
            case STAT_RECEIVED_SET:
            case STAT_PARSED_SET:
            case STAT_THROWN_AWAY_SET:
            case STAT_AGGREGATED_SET:
            case STAT_TIME_SPENT_AGGREGATING_ADD:
            case STAT_TIME_SPENT_PARSING_ADD:
                break;
            default:
                if (stat->data != NULL) {
                    free(stat->data);
                }
                break;
        }
        free(stat);
    }
}

/**
 * Creates arguments for PMDA stats collector thread
 * @arg data_extension - Struture which contains all priv data that is shared in all PCP procedures callbacks, also includes agent config and channels
 * @return pmda_stats_collector_args
 */
struct pmda_stats_collector_args*
create_pmda_stats_collector_args(struct pmda_data_extension* data_extension) {
    struct pmda_stats_collector_args* pmda_stats_collector_args = (struct pmda_stats_collector_args*) malloc(sizeof(struct pmda_stats_collector_args));
    ALLOC_CHECK("Unable to assign memory for pcp listener thread arguments.");
    pmda_stats_collector_args->data_extension = data_extension;
    return pmda_stats_collector_args;
}
