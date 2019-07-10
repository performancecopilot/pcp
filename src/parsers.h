#ifndef PARSERS_
#define PARSERS_

#include <chan/chan.h>

#include "config-reader.h"

struct parser_args
{
    struct agent_config* config;
    chan_t* unprocessed_datagrams;
    chan_t* parsed_datagrams;
    chan_t* stats_sink;
} parser_args;

struct statsd_datagram
{
    char* metric;
    char* type;
    char* tags;
    char* value;
    char* instance;
    char* sampling;
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
 * @arg unprocessed_channel - Network listener -> Parser
 * @arg parsed_channel - Parser -> Aggregator
 * @arg stats_sink - Channel for sending stats about PMDA itself
 * @return parser_args
 */
struct parser_args*
create_parser_args(struct agent_config* config, chan_t* unprocessed_channel, chan_t* parsed_channel, chan_t* stats_sink);

/**
 * Prints out parsed datagram structure in human readable form.
 */
void
print_out_datagram(struct statsd_datagram* datagram);

/**
 * Frees datagram
 */
void
free_datagram(struct statsd_datagram* datagram);

#endif
