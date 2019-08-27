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
#ifndef PARSERS_
#define PARSERS_

#include <chan/chan.h>

#include "network-listener.h"
#include "config-reader.h"

struct parser_args
{
    struct agent_config* config;
    chan_t* network_listener_to_parser;
    chan_t* parser_to_aggregator;
} parser_args;

enum METRIC_TYPE { 
    METRIC_TYPE_NONE = 0b00,
    METRIC_TYPE_COUNTER = 0b01,
    METRIC_TYPE_GAUGE = 0b10,
    METRIC_TYPE_DURATION = 0b11,
} METRIC_TYPE;

enum PARSER_RESULT_TYPE {
    PARSER_RESULT_PARSED = 0b00,
    PARSER_RESULT_DROPPED = 0b01,
    PARSER_RESULT_END = 0b11,
} PARSER_RESULT;

enum SIGN {
    SIGN_NONE,
    SIGN_PLUS,
    SIGN_MINUS,
} SIGN;

struct parser_to_aggregator_message
{
    struct statsd_datagram* data;
    enum PARSER_RESULT_TYPE type;
    unsigned long time;
} parser_to_aggregator_message;

struct statsd_datagram
{
    char* name;
    enum METRIC_TYPE type;
    char* tags;
    int tags_pair_count;
    enum SIGN explicit_sign;
    double value;
} statsd_datagram;

typedef int (*datagram_parse_callback)(char*, struct statsd_datagram**);

/**
 * Thread entrypoint - listens to incoming payload on a unprocessed channel and sends over successfully parsed data over to Aggregator thread via processed channel
 * @arg args - parser_args
 */
void*
parser_exec(void* args);

/**
 * Creates arguments for parser thread
 * @arg config - Application config
 * @arg network_listener_to_parser - Network listener -> Parser
 * @arg parser_to_aggregator - Parser -> Aggregator
 * @return parser_args
 */
struct parser_args*
create_parser_args(struct agent_config* config, chan_t* network_listener_to_parser, chan_t* parser_to_aggregator);

/**
 * 
 */
void
free_unprocessed_datagram(struct unprocessed_statsd_datagram* datagram);

/**
 * Frees datagram
 */
void
free_datagram(struct statsd_datagram* datagram);

#endif
