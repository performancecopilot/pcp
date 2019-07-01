#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include "ini.h"

/**
 * Returns default program config
 */
static struct agent_config*
get_default_config() {
    struct agent_config* config = (struct agent_config*) malloc(sizeof(struct agent_config));
    ALLOC_CHECK("Unable to allocate memory PMDA settings.");
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
    config->parser_type = PARSER_TYPE_BASIC;
    config->duration_aggregation_type = DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM; 
    return config;
}

/**
 * Read agent config from either file or command line arguments
 * @arg src_flag - Specifies config source, 0 = READ_FROM_FILE, 1 = READ_FROM_CMD
 * @arg config_path - Path to config file
 * @return Program configuration
 */
struct agent_config*
read_agent_config(int src_flag, char* config_path, int argc, char **argv) {
    struct agent_config* config = get_default_config();
    if (src_flag == READ_FROM_FILE) {
        read_agent_config_file(&config, config_path);
    } else if (src_flag == READ_FROM_CMD) {
        read_agent_config_cmd(&config, argc, argv);
    } else {
        DIE("Incorrect source flag for agent_config source.");
    }
    return config;
}

static int
ini_line_handler(void* user, const char* section, const char* name, const char* value) {
    (void)section;
    struct agent_config* dest = *(struct agent_config**) user;
    size_t length = strlen(value) + 1; 
    #define MATCH(x) strcmp(x, name) == 0
    if (MATCH("max_udp_packet_size")) {
        dest->max_udp_packet_size = strtoull(value, NULL, 10);
    } else if (MATCH("tcp_address")) {
        dest->tcp_address = (char*) malloc(length);
        ALLOC_CHECK("Unable to assign memory for config tcp address.");
        memcpy(dest->tcp_address, value, length);
    } else if (MATCH("port")) {
        dest->port = (char*) malloc(length);
        ALLOC_CHECK("Unable to allocate memory for config port number.");
        memcpy(dest->port, value, length);
    } else if (MATCH("verbose")) {
        dest->verbose = atoi(value);
    } else if (MATCH("debug")) {
        dest->debug = atoi(value);
    } else if (MATCH("debug_output_filename")) {
        dest->debug_output_filename = (char*) malloc(length);
        ALLOC_CHECK("Unable to asssing memory for config debug_output_filename");
        memcpy(dest->debug_output_filename, value, length);
    } else if (MATCH("version")) {
        dest->show_version = atoi(value);
    } else if (MATCH("trace")) {
        dest->trace = atoi(value);
    } else if (MATCH("parser_type")) {
        dest->parser_type = atoi(value);
    } else if (MATCH("duration_aggregation_type")) {
        dest->duration_aggregation_type = atoi(value);
    } else {
        return 0;
    }
    return 1;
}

/**
 * Reads program config from given path
 * @arg agent_config - Placeholder config to write what was read to
 * @arg path - Path to read file from
 */
void
read_agent_config_file(struct agent_config** dest, char* path) {
    if (access(path, F_OK) == -1) {
        DIE("No config file found on given path");
    }
    if (ini_parse(path, ini_line_handler, dest) < 0) {
        DIE("Can't load config file");
    }
    verbose_log("Config loaded from %s.", path);
}

/**
 * Reads program config from command line arguments
 * @arg agent_config - Placeholder config to write what was read to
 */
void
read_agent_config_cmd(struct agent_config** dest, int argc, char **argv) {
    int c;
    while(1) {
        static struct option long_options[] = {
            { "verbose", no_argument, 0, 1 },
            { "debug", no_argument, 0, 1 },
            { "version", no_argument, 0, 1 },
            { "trace", no_argument, 0, 1 },
            { "debug-output-filename", required_argument, 0, 'o' },
            { "max-udp", required_argument, 0, 'u' },
            { "tcpaddr", required_argument, 0, 't' },
            { "port", required_argument, 0, 'a' },
            { "parser-type", required_argument, 0, 'p'},
            { "duration-aggregation-type", required_argument, 0, 'd'},
            { 0, 0, 0, 0 }
        };
        int option_index = 0;
        c = getopt_long_only(argc, argv, "o::u::t::a::p::d::", long_options, &option_index);
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
            case 'd':
                (*dest)->duration_aggregation_type = atoi(optarg);
                break;
        }
    }
}

/**
 * Print out agent config to STDOUT
 * @arg config - Config to print out
 */
void
print_agent_config(struct agent_config* config) {
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
    printf("parser_type: %s \n", config->parser_type == PARSER_TYPE_BASIC ? "BASIC" : "RAGEL");
    printf("duration_aggregation_type: %s\n", 
        config->duration_aggregation_type == DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM ? "HDR_HISTOGRAM" : "BASIC");
    printf("---------------------------\n");
}
