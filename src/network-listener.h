#ifndef NETWORK_LISTENER_
#define NETWORK_LISTENER_

#include <pcp/dict.h>
#include <chan/chan.h>

#include "config-reader.h"

typedef struct unprocessed_statsd_datagram
{
    char* value;
} unprocessed_statsd_datagram;

typedef struct network_listener_args
{
    agent_config* config;
    chan_t* unprocessed_datagrams;
} network_listener_args;

/**
 * Thread entrypoint - listens on address and port specified in config 
 * for UDP/TCP containing StatsD payload and then sends it over to parser thread for parsing
 * @arg args - network_listener_args
 */
void* network_listener_exec(void* args);

/**
 * Packs up its arguments into struct so that we can pass it via single reference to the network listener thread
 */
network_listener_args* create_listener_args(agent_config* config, chan_t* unprocessed_channel);

#endif
