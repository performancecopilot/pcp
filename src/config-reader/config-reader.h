#ifndef CONFIG_READER_
#define CONFIG_READER_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

const int READ_FROM_FILE;
const int READ_FROM_CMD;

typedef enum PARSER_TYPE {
    TRIVIAL = 0b0,
    RAGEL = 0b1
} PARSER_TYPE;

typedef enum DURATION_AGGREGATION_TYPE {
    HDR_HISTOGRAM = 0b0,
    BASIC = 0b1
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

agent_config* read_agent_config(int src_flag, char* config_path, int argc, char **argv);

void read_agent_config_file(agent_config** dest, char* path);

void read_agent_config_cmd(agent_config** dest, int argc, char **argv);

void print_agent_config(agent_config* config);

#endif
