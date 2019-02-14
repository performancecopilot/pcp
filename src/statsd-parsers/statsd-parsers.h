#include "../config-reader/config-reader.h"

#ifndef STATSD_DATAGRAM_
#define STATSD_DATAGRAM_

const int PARSER_TRIVIAL;
const int PARSER_RAGEL;

typedef struct statsd_datagram
{
    char *data_namespace;
    char *val_str;
    char *modifier;
    double val_float;
    char *metric;
    char *sampling;
} statsd_datagram;

/*
 * Parses statsd datagrams based on provided agent_config 
 * with given parser_type
 * */
void statsd_parser_listen(agent_config* config, int parser_type, void (*callback)(statsd_datagram*));

void print_out_datagram(statsd_datagram* datagram);

#endif