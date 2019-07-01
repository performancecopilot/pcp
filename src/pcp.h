#ifndef PCP_
#define PCP_

#include <chan/chan.h>

#include "config-reader.h"

struct pcp_request {
    // not sure what to put here yet
} pcp_request;

struct pcp_args
{
    struct agent_config* config;
    chan_t* aggregator_request_channel;
    chan_t* aggregator_response_channel;
} pcp_args;

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
create_pcp_args(struct agent_config* config, chan_t* aggregator_request_channel, chan_t* aggregator_response_channel);

#endif
