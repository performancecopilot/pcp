#ifndef PARSERS_
#define PARSERS_

#include <chan/chan.h>

#include "config-reader.h"

typedef struct parser_args
{
    agent_config* config;
    chan_t* unprocessed_datagrams;
    chan_t* parsed_datagrams;
} parser_args;

typedef struct statsd_datagram
{
    char* metric;
    char* type;
    char* tags;
    char* value;
    char* instance;
    char* sampling;
} statsd_datagram;

/**
 * Thread entrypoint - listens to incoming payload on a unprocessed channel and sends over successfully parsed data over to Aggregator thread via processed channel
 * @arg args - parser_args
 */
void* parser_exec(void* args);

/**
 * Packs up its arguments into struct so that we can pass it via single reference to the parser thread
 */
parser_args* create_parser_args(agent_config* config, chan_t* unprocessed_channel, chan_t* parsed_channel);

/**
 * Prints out parsed datagram structure in human readable form.
 */
void print_out_datagram(statsd_datagram* datagram);

/**
 * Frees datagram
 */
void free_datagram(statsd_datagram* datagram);

#endif
