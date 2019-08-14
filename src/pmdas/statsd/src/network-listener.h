#ifndef NETWORK_LISTENER_
#define NETWORK_LISTENER_

#include <pcp/dict.h>
#include <chan/chan.h>

#include "config-reader.h"

struct unprocessed_statsd_datagram
{
    char* value;
} unprocessed_statsd_datagram;

struct network_listener_args
{
    struct agent_config* config;
    chan_t* network_listener_to_parser;
} network_listener_args;

/**
 * Thread entrypoint - listens on address and port specified in config 
 * for UDP/TCP containing StatsD payload and then sends it over to parser thread for parsing
 * @arg args - network_listener_args
 */
void*
network_listener_exec(void* args);

/**
 * Creates arguments for network listener thread
 * @arg config - Application config
 * @arg network_listener_to_parser - Network listener -> Parser
 * @return network_listener_args
 */
struct network_listener_args*
create_listener_args(struct agent_config* config, chan_t* network_listener_to_parser);

#endif
