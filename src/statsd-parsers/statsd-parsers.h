#include "../config-reader/config-reader.h"

#ifndef STATSD_DATAGRAM_
#define STATSD_DATAGRAM_

const int PARSER_TRIVIAL;
const int PARSER_RAGEL;

typedef struct statsd_datagram
{
    char *data_namespace;
    char *type;
    char *modifier;
    char *tags;
    double value;
    char *metric;
    char *instance;
    char *sampling;
} statsd_datagram;

/*
 * Parses statsd datagrams based on provided agent_config 
 * with given parser_type
 * */
void statsd_parser_listen(agent_config* config, int parser_type, void (*callback)(statsd_datagram*));

void print_out_datagram(statsd_datagram* datagram);

void free_datagram(statsd_datagram* datagram);

#endif