#ifndef STATSD_PARSERS_
#define STATSD_PARSERS_

#include <pcp/dict.h>
#include <chan/chan.h>

#include "../config-reader/config-reader.h"

typedef dict metrics;

const int PARSER_TRIVIAL;
const int PARSER_RAGEL;

typedef struct tag {
    char* key;
    char* value;
} tag;

typedef struct tag_collection {
    tag** values;
    long int length;
} tag_collection;

typedef struct statsd_datagram
{
    char* metric;
    char* type;
    char* tags;
    char* value;
    char* instance;
    char* sampling;
} statsd_datagram;

typedef struct unprocessed_statsd_datagram
{
    char* value;
} unprocessed_statsd_datagram;

typedef struct statsd_listener_args
{
    agent_config* config;
    chan_t* unprocessed_datagrams;
} statsd_listener_args;

statsd_listener_args* create_listener_args(agent_config* config, chan_t* unprocessed_channel);

void* statsd_network_listen(void* args);

typedef struct statsd_parser_args
{
    agent_config* config;
    chan_t* unprocessed_datagrams;
    chan_t* parsed_datagrams;
} statsd_parser_args;

statsd_parser_args* create_parser_args(agent_config* config, chan_t* unprocessed_channel, chan_t* parsed_channel);

typedef struct consumer_args
{
    agent_config* config;
    chan_t* parsed_datagrams;
    metrics* metrics_wrapper;
} consumer_args;

consumer_args* create_consumer_args(agent_config* config, chan_t* parsed_channel, metrics* m);

void* statsd_parser_consume(void* args);

void print_out_datagram(statsd_datagram* datagram);

void free_datagram(statsd_datagram* datagram);

#endif