#ifndef PMDA_STATS_COLLECTOR_
#define PMDA_STATS_COLLECTOR_

#include "pcp.h"

enum STAT_MESSAGE_TYPE {
    STAT_RECEIVED_INC,
    STAT_PARSED_INC,
    STAT_THROWN_AWAY_INC,
    STAT_AGGREGATED_INC,
    STAT_RECEIVED_SET,
    STAT_PARSED_SET,
    STAT_THROWN_AWAY_SET,
    STAT_AGGREGATED_SET,
    STAT_TIME_SPENT_PARSING_ADD,
    STAT_TIME_SPENT_AGGREGATING_ADD,
} STAT_MESSAGE_TYPE;

struct stat_message {
    enum STAT_MESSAGE_TYPE type;
    void* data;
} stat_message;

struct pmda_stats_collector_args {
    struct pmda_data_extension* data_extension;
} pmda_stats_collector_args;

/**
 * Main loop handling incoming PMDA stats
 * @arg args - pmda_stats_collector_args
 */
void*
pmda_stats_collector_exec(void* args);

/**
 * Creates Stat message
 * @arg type - Type of stat message
 * @arg data - Arbitrary user data which kind is defined by the type of stat message
 * @return new stat message construct 
 */
struct stat_message*
create_stat_message(enum STAT_MESSAGE_TYPE type, void* data);

/**
 * Frees up stat message
 * @arg stat - Stat message to be freed
 */
void
free_stat_message(struct stat_message* stat);

/**
 * Creates arguments for PMDA stats collector thread
 * @arg data_extension - Struture which contains all priv data that is shared in all PCP procedures callbacks, also includes agent config and channels
 * @return pmda_stats_collector_args
 */
struct pmda_stats_collector_args*
create_pmda_stats_collector_args(struct pmda_data_extension* data_extension);

#endif
