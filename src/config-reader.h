#ifndef CONFIG_READER_
#define CONFIG_READER_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

const int READ_FROM_FILE;
const int READ_FROM_CMD;

typedef enum PARSER_TYPE {
    PARSER_TYPE_BASIC = 0b0,
    PARSER_TYPE_RAGEL = 0b1
} PARSER_TYPE;

typedef enum DURATION_AGGREGATION_TYPE {
    DURATION_AGGREGATION_TYPE_BASIC = 0b0,
    DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM = 0b1
} DURATION_AGGREGATION_TYPE;

typedef struct agent_config {
    uint64_t max_udp_packet_size;
    int32_t tcp_read_size;
    int32_t max_unprocessed_packets;
    int verbose;
    int debug;
    DURATION_AGGREGATION_TYPE duration_aggregation_type;
    char* debug_output_filename;
    int trace;
    PARSER_TYPE parser_type;
    int show_version;
    char* port;
    char* tcp_address;
} agent_config;

/**
 * Read agent config from either file or command line arguments
 * @arg src_flag - Specifies config source, 0 = READ_FROM_FILE, 1 = READ_FROM_CMD
 * @arg config_path - Path to config file
 * @return Program configuration
 */
agent_config* read_agent_config(int src_flag, char* config_path, int argc, char **argv);

/**
 * Reads program config from given path
 * @arg agent_config - Placeholder config to write what was read to
 * @arg path - Path to read file from
 */
void read_agent_config_file(agent_config** dest, char* path);

/**
 * Reads program config from command line arguments
 * @arg agent_config - Placeholder config to write what was read to
 */
void read_agent_config_cmd(agent_config** dest, int argc, char **argv);

/**
 * Print out agent config to STDOUT
 * @arg config - Config to print out
 */
void print_agent_config(agent_config* config);

#endif
