#include <stdio.h>
#include <stdlib.h>

#include "config-reader/config-reader.h"
#include "statsd-parsers/statsd-parsers.h"

struct hdr_histogram* histogram;

int main(int argc, char **argv)
{
    agent_config* config = (agent_config*) malloc(sizeof(agent_config));
    int config_src_type = argc >= 2 ? READ_FROM_CMD : READ_FROM_FILE;
    config = read_agent_config(config_src_type, "config", argc, argv);
    print_agent_config(config);
    statsd_parser_listen(config, config->parser_type, print_out_datagram);
    return 1;
}