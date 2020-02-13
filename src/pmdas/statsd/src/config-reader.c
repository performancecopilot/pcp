/*
 * Copyright (c) 2019 Miroslav Foltýn.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#include <string.h>
#include <unistd.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <pcp/ini.h>

#include "config-reader.h"
#include "utils.h"

/**
 * Returns default program config
 */
static void
set_default_config(struct agent_config* config) {
    config->max_udp_packet_size = 1472;
    config->max_unprocessed_packets = 2048;
    config->verbose = 0;
    config->debug_output_filename = "debug";
    config->show_version = 0;
    config->port = 8125;
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
        long unsigned int param = strtoul(value, NULL, 10);
        if (param < UINT32_MAX) {
            dest->max_udp_packet_size = (unsigned int) param;
        }
    } else if(MATCH("max_unprocessed_packets")) {
        long unsigned int param = strtoul(value, NULL, 10);
        if (param < UINT32_MAX) {
            dest->max_unprocessed_packets = (unsigned int) param;
        }
    } else if (MATCH("port")) {
        long unsigned int param = strtoul(value, NULL, 10);
        if (param < UINT32_MAX) {
            dest->port = (unsigned int) param;
        }
    } else if (MATCH("verbose")) {
        long unsigned int param = strtoul(value, NULL, 10);
        if (param < 3) {
            dest->verbose = param;
        }
    } else if (MATCH("debug_output_filename")) {
        dest->debug_output_filename = (char*) malloc(length);
        ALLOC_CHECK("Unable to asssing memory for config debug_output_filename");
        memcpy(dest->debug_output_filename, value, length);
    } else if (MATCH("version")) {
        long unsigned int param = strtoul(value, NULL, 10);
        if (param < UINT32_MAX) {
            dest->show_version = (unsigned int) param;
        }
    } else if (MATCH("parser_type")) {
        long unsigned int param = strtoul(value, NULL, 10);
        if (param < UINT32_MAX) {
            dest->parser_type = (unsigned int) param;
        }
    } else if (MATCH("duration_aggregation_type")) {
        long unsigned int param = strtoul(value, NULL, 10);
        if (param < UINT32_MAX) {
            dest->duration_aggregation_type = (unsigned int) param;
        }
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
    pmNotifyErr(LOG_INFO, "Config loaded from %s.", path);
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
        { "verbose", 1, 'v', "VERBOSE", "Verbose logging" },
        { "version", 0, 's', "VERSION", "Display version" },
        { "debug-output-filename", 1, 'o', "DEBUG-OUTPUT-FILENAME", "Debug file output path" },
        { "max-udp", 1, 'Z', "MAX-UDP", "Maximum size of UDP datagram" },
        { "port", 1, 'P', "PORT", "Port to listen to" },
        { "parser-type", 1, 'r', "PARSER-TYPE", "Parser type to use (ragel = 1, basic = 0)" },
        { "duration-aggregation-type", 1, 'a', "DURATION-AGGREGATION-TYPE", "Aggregation type for duration metric to use (hdr_histogram = 1, basic histogram = 0)" },
        { "max-unprocessed-packets-size:", 1, 'z', "MAX-UNPROCESSED-PACKETS-SIZE", "Maximum count of unprocessed packets." },
        PMDA_OPTIONS_END
    };

    static pmdaOptions opts = {
        .short_options = "D:d:l:U:v:so:Z:P:r:a:z:?",
        .long_options = longopts,
    };
    while(1) {
        c = pmdaGetOptions(argc, argv, &opts, dispatch);
        if (c == -1) break;
        switch (c) {
            case 'v':
            {
                long unsigned int param = strtoul(opts.optarg, NULL, 10);
                if (param < 3) {
                    dest->verbose = (unsigned int) param;
                } else {
                    pmNotifyErr(LOG_INFO, "verbose option value is out of bounds.");
                }
                break;
            }
            case 's':
                dest->show_version = 1;
                break;
            case 'o':
                dest->debug_output_filename = opts.optarg;
                break;
            case 'Z':
            {
                long unsigned int param = strtoul(opts.optarg, NULL, 10);
                if (param < UINT32_MAX) {
                    dest->max_udp_packet_size = param;
                } else {
                    pmNotifyErr(LOG_INFO, "max_udp_packet_size option value is out of bounds.");
                }
                break;
            }
            case 'P':
            {
                long unsigned int param = strtoul(opts.optarg, NULL, 10);		
                if (param < 65535) {		
                    dest->port = (unsigned int) param;		
                } else {
                    pmNotifyErr(LOG_INFO, "port option value is out of bounds.");
                }
                break;
            }
            case 'r':
            {
                long unsigned int param = strtoul(opts.optarg, NULL, 10);		
                if (param < UINT32_MAX) {		
                    dest->parser_type = (unsigned int) param;		
                } else {
                    pmNotifyErr(LOG_INFO, "parser_type option value is out of bounds.");
                }
                break;
            }
            case 'a':
            {
                long unsigned int param = strtoul(opts.optarg, NULL, 10);		
                if (param < UINT32_MAX) {		
                    dest->duration_aggregation_type = (unsigned int) param;		
                } else {
                    pmNotifyErr(LOG_INFO, "duration_aggregation_type option value is out of bounds.");
                }
                break;
            }
            case 'z':
            {
                long unsigned int param = strtoul(opts.optarg, NULL, 10);		
                if (param < UINT32_MAX) {		
                    dest->max_unprocessed_packets = (unsigned int) param;		
                } else {
                    pmNotifyErr(LOG_INFO, "max_unprocessed_packets option value is out of bounds.");
                }
                break;
            }
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
    pmNotifyErr(LOG_INFO, "<settings>\n");
    pmNotifyErr(LOG_INFO, "verbosity: %d", config->verbose);
    if (config->show_version)
        pmNotifyErr(LOG_INFO, "version flag is set");
    pmNotifyErr(LOG_INFO, "debug_output_filename: %s \n", config->debug_output_filename);
    pmNotifyErr(LOG_INFO, "port: %d \n", config->port);
    pmNotifyErr(LOG_INFO, "parser_type: %s \n", config->parser_type == PARSER_TYPE_BASIC ? "BASIC" : "RAGEL");
    pmNotifyErr(LOG_INFO, "maximum of unprocessed packets: %d \n", config->max_unprocessed_packets);
    pmNotifyErr(LOG_INFO, "maximum udp packet size: %ld \n", config->max_udp_packet_size);
    pmNotifyErr(LOG_INFO, "duration_aggregation_type: %s\n", 
        config->duration_aggregation_type == DURATION_AGGREGATION_TYPE_HDR_HISTOGRAM ? "HDR_HISTOGRAM" : "BASIC");
    pmNotifyErr(LOG_INFO, "</settings>\n");
}
