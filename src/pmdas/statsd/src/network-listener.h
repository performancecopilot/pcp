/*
 * Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
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
 * Free unprocessed datagram
 * @arg datagram
 */
void
free_unprocessed_datagram(struct unprocessed_statsd_datagram* datagram);

/**
 * Creates arguments for network listener thread
 * @arg config - Application config
 * @arg network_listener_to_parser - Network listener -> Parser
 * @return network_listener_args
 */
struct network_listener_args*
create_listener_args(struct agent_config* config, chan_t* network_listener_to_parser);

#endif
