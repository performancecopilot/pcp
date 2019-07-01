#ifndef CONFIG_READER_
#define CONFIG_READER_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/**
 * Flags for available config source
 */
enum READ_SOURCE_TYPE {
    READ_FROM_FILE = 0,
    READ_FROM_CMD = 1,
} READ_SOURCE_TYPE;

enum PARSER_TYPE {
    PARSER_TYPE_BASIC = 0,
    PARSER_TYPE_RAGEL = 1
} PARSER_TYPE;

enum DURATION_AGGREGATION_TYPE {
    DURATION_AGGREGATION_TYPE_BASIC = 0,
    DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM = 1
} DURATION_AGGREGATION_TYPE;

struct agent_config {
    enum DURATION_AGGREGATION_TYPE duration_aggregation_type;
    enum PARSER_TYPE parser_type;
    int verbose;
    int debug;
    int trace;
    int show_version;
    int32_t tcp_read_size;
    int32_t max_unprocessed_packets;
    uint64_t max_udp_packet_size;
    char* debug_output_filename;
    char* port;
    char* tcp_address;
} agent_config;

/**
 * Read agent config from either file or command line arguments
 * @arg src_flag - Specifies config source, 0 = READ_FROM_FILE, 1 = READ_FROM_CMD
 * @arg config_path - Path to config file
 * @return Program configuration
 */
struct agent_config*
read_agent_config(int src_flag, char* config_path, int argc, char **argv);

/**
 * Reads program config from given path
 * @arg agent_config - Placeholder config to write what was read to
 * @arg path - Path to read file from
 */
void
read_agent_config_file(struct agent_config** dest, char* path);

/**
 * Reads program config from command line arguments
 * @arg agent_config - Placeholder config to write what was read to
 */
void
read_agent_config_cmd(struct agent_config** dest, int argc, char **argv);

/**
 * Print out agent config to STDOUT
 * @arg config - Config to print out
 */
void
print_agent_config(struct agent_config* config);

#endif
