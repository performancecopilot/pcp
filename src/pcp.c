#include <chan/chan.h>
#include "statsd-parsers.h"
#include "pcp.h"

/**
 * NOT IMPLEMENTED
 * Main loop handling incoming responses from aggregators
 * @arg args - (pcp_args), see ~/src/statsd-parsers.h
 */
void* pcp_pmda(void* args) {
    agent_config* config = ((pcp_args*)args)->config;
    chan_t* pcp_to_aggregator = ((pcp_args*)args)->aggregator_request_channel;
    chan_t* aggregator_to_pcp = ((pcp_args*)args)->aggregator_response_channel;
    (void)config;
    (void)pcp_to_aggregator;
    (void)aggregator_to_pcp;

    while(1) {} // Will probably request/respond here
}
