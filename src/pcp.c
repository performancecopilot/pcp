#include <chan/chan.h>

#include "pcp.h"
#include "utils.h"

/**
 * NOT IMPLEMENTED
 * Main loop handling incoming responses from aggregators
 * @arg args - (pcp_args), see ~/src/network-listener.h
 */
void* pcp_pmda_exec(void* args) {
    agent_config* config = ((pcp_args*)args)->config;
    chan_t* pcp_to_aggregator = ((pcp_args*)args)->aggregator_request_channel;
    chan_t* aggregator_to_pcp = ((pcp_args*)args)->aggregator_response_channel;
    (void)config;
    (void)pcp_to_aggregator;
    (void)aggregator_to_pcp;

    while(1) {} // Will probably request/respond here
}

/**
 * Creates arguments for PCP thread
 * @arg config - Application config
 * @arg aggregator_request_channel - Aggregator -> PCP channel
 * @arg aggregator_response_channel - PCP -> Aggregator channel
 * @return pcp_args
 */
pcp_args* create_pcp_args(agent_config* config, chan_t* aggregator_request_channel, chan_t* aggregator_response_channel) {
    struct pcp_args* pcp_args = (struct pcp_args*) malloc(sizeof(struct pcp_args));
    ALLOC_CHECK("Unable to assign memory for pcp thread arguments.");
    pcp_args->config = (agent_config*) malloc(sizeof(agent_config*));
    ALLOC_CHECK("Unable to assign memory for pcp thread config.");
    pcp_args->config = config;
    pcp_args->aggregator_request_channel = aggregator_request_channel;
    pcp_args->aggregator_response_channel = aggregator_response_channel;
    return pcp_args;
}
