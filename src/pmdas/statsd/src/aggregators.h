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
#ifndef AGGREGATORS_
#define AGGREGATORS_

#include <stddef.h>
#include <pcp/dict.h>
#include <chan/chan.h>

#include "config-reader.h"
#include "parsers.h"
#include "network-listener.h"

struct aggregator_args
{
    struct agent_config* config;
    chan_t* parser_to_aggregator;
    struct pmda_metrics_container* metrics_container;
    struct pmda_stats_container* stats_container;
} aggregator_args;

/**
 * Thread startpoint - passes down given datagram to aggregator to record value it contains (should be used for a single new thread)
 * @arg args - aggregator_args
 */
void*
aggregator_exec(void* args);

/**
 * Outputs debug info
 */
void
aggregator_debug_output();

/**
 * Frees pointer to aggregator message
 * @arg message - Message to be freed
 */
void
free_parser_to_aggregator_message(struct parser_to_aggregator_message* message);

/**
 * Creates arguments for Agregator thread
 * @arg config - Application config
 * @arg parsed_channel - Parser -> Aggregator channel
 * @arg pcp_request_channel - PCP -> Aggregator channel
 * @arg pcp_response_channel - Aggregator -> PCP channel
 * @arg stats_sink - Channel for sending stats about PMDA itself
 * @return aggregator_args
 */
struct aggregator_args*
create_aggregator_args(
    struct agent_config* config,
    chan_t* parser_to_aggregator,
    struct pmda_metrics_container* m,
    struct pmda_stats_container* s
);

#endif
