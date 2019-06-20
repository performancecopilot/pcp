#ifndef PCP_
#define PCP_

typedef struct pcp_request {
    // not sure what to put here yet
} pcp_request;

/**
 * NOT IMPLEMENTED
 * Main loop handling incoming responses from aggregators
 * @arg args - (pcp_args), see ~/src/statsd-parsers.h
 */
void* pcp_pmda(void* args);

#endif
