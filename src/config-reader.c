#include <string.h>
#include <unistd.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#include "config-reader.h"
#include "utils.h"
#include "ini.h"

/**
 * Returns default program config
 */
static void
set_default_config(struct agent_config* config) {
    config->max_udp_packet_size = 1472;
    config->max_unprocessed_packets = 2048;
    config->verbose = 0;
    config->debug = 0;
    config->debug_output_filename = "debug";
    config->show_version = 0;
    config->port = (char *) "8125";
    config->tcp_address = (char *) "0.0.0.0";
    config->parser_type = PARSER_TYPE_BASIC;
    config->duration_aggregation_type = DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM;
    pmGetUsername(&(config->username));
}

/**
 * Read agent config from either file or command line arguments
 * @arg src_flag - Specifies config source, 0 = READ_FROM_FILE, 1 = READ_FROM_CMD
 * @arg config_path - Path to config file
 */
void
read_agent_config(struct agent_config* config, pmdaInterface* dispatch, char* config_path, int argc, char **argv) {
    set_default_config(config);
    read_agent_config_file(config, config_path);
    read_agent_config_cmd(dispatch, config, argc, argv);
}           

static int
ini_line_handler(void* user, const char* section, const char* name, const char* value) {
    (void)section;
    struct agent_config* dest = (struct agent_config*) user;
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
read_agent_config_file(struct agent_config* dest, char* path) {
    if (access(path, F_OK) == -1) {
        DIE("No config file found on given path");
    }
    if (ini_parse(path, ini_line_handler, dest) < 0) {
        DIE("Can't load config file");
    }
    VERBOSE_LOG("Config loaded from %s.", path);
}

/**
 * Reads program config from command line arguments
 * @arg agent_config - Placeholder config to write what was read to
 */
void
read_agent_config_cmd(pmdaInterface* dispatch, struct agent_config* dest, int argc, char **argv) {
    int c;
    static pmLongOptions longopts[] = {
        PMDA_OPTIONS_HEADER("Options"),
        PMOPT_DEBUG,
        PMDAOPT_DOMAIN,
        PMDAOPT_LOGFILE,
        PMDAOPT_USERNAME,
        PMOPT_HELP,
        { "verbose", 0, 'v', "VERBOSE", "Verbose logging" },
        { "debug", 0, 'g', "DEBUG", "Debug logging" },
        { "version", 0, 's', "VERSION", "Display version" },
        { "debug-output-filename", 1, 'o', "DEBUG-OUTPUT-FILENAME", "Debug file output path" },
        { "max-udp", 1, 'Z', "MAX-UDP", "Maximum size of UDP datagram" },
        { "tcp-address", 1, 't', "TCP-ADDRESS", "TCP address to listen to" },
        { "port", 1, 'P', "PORT", "Port to listen to" },
        { "parser-type", 1, 'r', "PARSER-TYPE", "Parser type to use (ragel = 1, basic = 0)" },
        { "duration-aggregation-type", 1, 'a', "DURATION-AGGREGATION-TYPE", "Aggregation type for duration metric to use (hdr_histogram = 1, basic histogram = 0)" },
        { "max-unprocessed-packets-size:", 1, 'z', "MAX-UNPROCESSED-PACKETS-SIZE", "Maximum count of unprocessed packets." },
        PMDA_OPTIONS_END
    };

    static pmdaOptions opts = {
        .short_options = "D:d:l:U:vgso:Z:t:P:r:a:z:?",
        .long_options = longopts,
    };
    while(1) {
        c = pmdaGetOptions(argc, argv, &opts, dispatch);
        if (c == -1) break;
        switch (c) {
            case 'v':
                dest->verbose = 1;
                break;
            case 'g':
                dest->debug = 1;
                break;
            case 's':
                dest->show_version = 1;
                break;
            case 'o':
                printf("output: %s \n", opts.optarg);
                dest->debug_output_filename = opts.optarg;
                break;
            case 'Z':
                printf("output: %s \n", opts.optarg);
                dest->max_udp_packet_size = strtoll(opts.optarg, NULL, 10);
                break;
            case 't':
                dest->tcp_address = opts.optarg;
                break;
            case 'P':
                dest->port = opts.optarg;
                break;
            case 'r':
                dest->parser_type = atoi(opts.optarg);
                break;
            case 'a':
                dest->duration_aggregation_type = atoi(opts.optarg);
                break;
            case 'z':
                dest->max_unprocessed_packets = atoi(opts.optarg);
                break;
        }
    }
    if (opts.errors) {
        pmdaUsageMessage(&opts);
        exit(1);
    }
    if (opts.username) {
        dest->username = opts.username;
    }
}

/**
 * Print out agent config to STDOUT
 * @arg config - Config to print out
 */
void
print_agent_config(struct agent_config* config) {
    pmNotifyErr(LOG_DEBUG, "<settings>\n");
    if (config->verbose)
        pmNotifyErr(LOG_DEBUG, "verbose flag is set");
    if (config->debug)
        pmNotifyErr(LOG_DEBUG, "debug flag is set");
    if (config->show_version)
        pmNotifyErr(LOG_DEBUG, "version flag is set");
    pmNotifyErr(LOG_DEBUG, "debug_output_filename: %s \n", config->debug_output_filename);
    pmNotifyErr(LOG_DEBUG, "tcpaddr: %s \n", config->tcp_address);
    pmNotifyErr(LOG_DEBUG, "port: %s \n", config->port);
    pmNotifyErr(LOG_DEBUG, "parser_type: %s \n", config->parser_type == PARSER_TYPE_BASIC ? "BASIC" : "RAGEL");
    pmNotifyErr(LOG_DEBUG, "maximum of unprocessed packets: %d \n", config->max_unprocessed_packets);
    pmNotifyErr(LOG_DEBUG, "maximum udp packet size: %ld \n", config->max_udp_packet_size);
    pmNotifyErr(LOG_DEBUG, "duration_aggregation_type: %s\n", 
        config->duration_aggregation_type == DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM ? "HDR_HISTOGRAM" : "BASIC");
    pmNotifyErr(LOG_DEBUG, "</settings>\n");
}
