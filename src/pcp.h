#ifndef PCP_
#define PCP_

#include <chan/chan.h>

#include "config-reader.h"

struct pcp_request {
    // not sure what to put here yet
} pcp_request;

struct pcp_args {
    struct agent_config* config;
    chan_t* aggregator_request_channel;
    chan_t* aggregator_response_channel;
    chan_t* stats_sink;
    int argc;
    char** argv;
} pcp_args;

enum STAT_MESSAGE_TYPE {
    STAT_RECEIVED_INC,
    STAT_RECEIVED_RESET,
    STAT_PARSED_INC,
    STAT_PARSED_RESET,
    STAT_THROWN_AWAY_INC,
    STAT_THROWN_AWAY_RESET,
    STAT_AGGREGATED_INC,
    STAT_AGGREGATED_RESET,
} STAT_MESSAGE_TYPE;

struct stat_message {
    enum STAT_MESSAGE_TYPE type;
    void* data;
} stat_message;

/**
 * Create Stat message
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
 * NOT IMPLEMENTED
 * Main loop handling incoming responses from aggregators
 * @arg args - Arguments passed to the thread
 */
void*
pcp_pmda_exec(void* args);

/**
 * Creates arguments for PCP thread
 * @arg config - Application config
 * @arg aggregator_request_channel - Aggregator -> PCP channel
 * @arg aggregator_response_channel - PCP -> Aggregator channel
 * @return pcp_args
 */
struct pcp_args*
create_pcp_args(
    struct agent_config* config,
    chan_t* aggregator_request_channel,
    chan_t* aggregator_response_channel,
    chan_t* stats_sink,
    int argc,
    char** argv
);

#endif
