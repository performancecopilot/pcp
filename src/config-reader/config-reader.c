#include "config-reader.h"
#include "../utils/utils.h"
#include <getopt.h>
#include <string.h>
#include <unistd.h>

/**
 * Flags for available config source
 */
#define READ_FROM_FILE 0
#define READ_FROM_CMD 1

/**
 * Flags for available parser types
 */
#define PARSER_TRIVIAL 0
#define PARSER_RAGEL 1

/**
 * Returns default program config
 */
static agent_config* get_default_config() {
    agent_config* config = (agent_config*) malloc(sizeof(agent_config));
    config->max_udp_packet_size = 1472;
    config->tcp_read_size = 4096;
    config->max_unprocessed_packets = 2048;
    config->verbose = 0;
    config->debug = 0;
    config->debug_output_filename = "debug";
    config->trace = 0;
    config->show_version = 0;
    config->port = (char *) "8125";
    config->tcp_address = (char *) "0.0.0.0";
    config->parser_type = PARSER_TRIVIAL;
    return config;
}

/**
 * Read agent config from either file or command line arguments
 * @arg src_flag - Specifies config source, 0 = READ_FROM_FILE, 1 = READ_FROM_CMD
 * @arg config_path - Path to config file
 * @return Program configuration
 */
agent_config* read_agent_config(int src_flag, char* config_path, int argc, char **argv) {
    agent_config* config = get_default_config();
    if (!(src_flag == READ_FROM_CMD || src_flag == READ_FROM_FILE)) {
        die(__LINE__, "Incorrect source flag for agent_config source.");
    }
    if (src_flag == READ_FROM_FILE) {
        read_agent_config_file(&config, config_path);
    } else {
        read_agent_config_cmd(&config, argc, argv);
    }
    return config;
}

/**
 * Reads program config from given path
 * @arg agent_config - Placeholder config to write what was read to
 * @arg path - Path to read file from
 */
void read_agent_config_file(agent_config** dest, char* path) {
    char line_buffer[256];
    int line_index = 0;
    if (access(path, F_OK) == -1) {
        die(__LINE__, "No config file found on given path");
    }
    FILE *config = fopen(path, "r");
    char* option = (char *) malloc(256);
    ALLOC_CHECK("Unable to assign memory for parsing options file.");
    char* value = (char *) malloc(256);
    ALLOC_CHECK("Unable to assing memory fo parsing options files.");
    const char MAX_UDP_PACKET_SIZE_OPTION[] = "max_udp_packet_size";
    const char PORT_OPTION[] = "port";
    const char TCP_ADDRESS_OPTION[] = "tcp_address";
    const char VERSION_OPTION[] = "version";
    const char VERBOSE_OPTION[] = "verbose";
    const char TRACE_OPTION[] = "trace";
    const char DEBUG_OPTION[] = "debug";
    const char DEBUG_OUTPUT_FILENAME[] = "debug_output_filename";
    const char PARSER_TYPE_OPTION[] = "parser_type";
    while (fgets(line_buffer, 256, config) != NULL) {
        if (sscanf(line_buffer, "%s %s", option, value) != 2) {
            die(__LINE__, "Syntax error in config file on line %d", line_index + 1);
        }
        if (strcmp(option, MAX_UDP_PACKET_SIZE_OPTION) == 0) {
            (*dest)->max_udp_packet_size = strtoull(value, NULL, 10);
        } else if (strcmp(option, PORT_OPTION) == 0) {
            (*dest)->port = (char *) malloc(strlen(value));
            ALLOC_CHECK("Unable to assign memory for port number.");
            strncat((*dest)->port, value, strlen(value));
        } else if (strcmp(option, TCP_ADDRESS_OPTION) == 0) {
            (*dest)->tcp_address = (char *) malloc(strlen(value));
            ALLOC_CHECK("Unable to assign memory for tcp address.");
            strncat((*dest)->tcp_address, value, strlen(value));
        } else if (strcmp(option, VERBOSE_OPTION) == 0) {
            (*dest)->verbose = atoi(value);
        } else if (strcmp(option, DEBUG_OPTION) == 0) {
            (*dest)->debug = atoi(value);
        } else if (strcmp(option, DEBUG_OUTPUT_FILENAME) == 0) {
            (*dest)->debug_output_filename = (char *) malloc(strlen(value));
            ALLOC_CHECK("Unable to assign memory for debug output path.");
            strncat((*dest)->debug_output_filename, value, strlen(value));
        } else if (strcmp(option, VERSION_OPTION) == 0) {
            (*dest)->show_version = atoi(value);
        } else if (strcmp(option, TRACE_OPTION) == 0) {
            (*dest)->trace = atoi(value);
        } else if (strcmp(option, PARSER_TYPE_OPTION) == 0) {
            (*dest)->parser_type = atoi(value);
        }
        line_index++;
        memset(option, '\0', 256);
        memset(value, '\0', 256);
    }
    free(value);
    free(option);
    fclose(config);
}

/**
 * Reads program config from command line arguments
 * @arg agent_config - Placeholder config to write what was read to
 */
void read_agent_config_cmd(agent_config** dest, int argc, char **argv) {
    int c;
    while(1) {
        static struct option long_options[] = {
            { "verbose", no_argument, 0, 1 },
            { "debug", no_argument, 0, 1 },
            { "version", no_argument, 0, 1 },
            { "trace", no_argument, 0, 1 },
            { "debug_output_filename", required_argument, 0, 'o' },
            { "max-udp", required_argument, 0, 'u' },
            { "tcpaddr", required_argument, 0, 't' },
            { "port", required_argument, 0, 'a' },
            { "parser-type", required_argument, 0, 'p'},
            { 0, 0, 0, 0 }
        };
        int option_index = 0;
        c = getopt_long_only(argc, argv, "o::u::t::a::p::", long_options, &option_index);
        if (c == -1) break;
        switch (c) {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
                verbose_log("option %s:", long_options[option_index].name);
                if (optarg)
                    verbose_log(" with arg %s", optarg);
                break;
            case 'u':
                (*dest)->max_udp_packet_size = strtoll(optarg, NULL, 10);
                break;
            case 't':
                (*dest)->tcp_address = optarg;
                break;
            case 'a':
                (*dest)->port = optarg;
                break;
            case 'o':
                (*dest)->debug_output_filename = optarg;
                break;
            case 'p':
                (*dest)->parser_type = atoi(optarg);
                break;
        }
    }
}

/**
 * Print out agent config to STDOUT
 * @arg config - Config to print out
 */
void print_agent_config(agent_config* config) {
    printf("---------------------------\n");
    if (config->verbose)
        puts("verbose flag is set");
    if (config->debug)
        puts("debug flag is set");
    if (config->show_version)
        puts("version flag is set");
    printf("debug_output_filename: %s \n", config->debug_output_filename);
    printf("maxudp: %lu \n", config->max_udp_packet_size);
    printf("tcpaddr: %s \n", config->tcp_address);
    printf("port: %s \n", config->port);
    printf("parser_type: %s \n", config->parser_type == 0 ? "TRIVIAL" : "RAGEL");
    printf("---------------------------\n");
}
