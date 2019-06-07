#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef CONFIG_READER_
#define CONFIG_READER_

const int READ_FROM_FILE;
const int READ_FROM_CMD;

typedef struct agent_config {
    uint64_t max_udp_packet_size;
    int32_t tcp_read_size;
    int32_t max_unprocessed_packets;
    int verbose;
    int debug;
    char* debug_output_filename;
    int trace;
    int parser_type;
    int show_version;
    char* port;
    char* tcp_address;
} agent_config;

agent_config* read_agent_config(int src_flag, char* config_path, int argc, char **argv);

void read_agent_config_file(agent_config** dest, char* path);

void read_agent_config_cmd(agent_config** dest, int argc, char **argv);

void print_agent_config(agent_config* config);

#endif
