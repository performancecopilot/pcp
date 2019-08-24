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
#ifndef CONFIG_READER_
#define CONFIG_READER_

#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
    unsigned long int max_udp_packet_size;
    unsigned int verbose;
    unsigned int show_version;
    unsigned int max_unprocessed_packets;
    unsigned int port;
    char* debug_output_filename;
    char* username;
} agent_config;

/**
 * Read agent config from either file or command line arguments
 * @arg src_flag - Specifies config source, 0 = READ_FROM_FILE, 1 = READ_FROM_CMD
 * @arg config_path - Path to config file
 */
void
read_agent_config(struct agent_config* config, pmdaInterface* dispatch, char* config_path, int argc, char **argv);

/**
 * Reads program config from given path
 * @arg agent_config - Placeholder config to write what was read to
 * @arg path - Path to read file from
 */
void
read_agent_config_file(struct agent_config* dest, char* path);

/**
 * Reads program config from command line arguments
 * @arg agent_config - Placeholder config to write what was read to
 */
void
read_agent_config_cmd(pmdaInterface* dispatch, struct agent_config* dest, int argc, char **argv);

/**
 * Print out agent config to STDOUT
 * @arg config - Config to print out
 */
void
print_agent_config(struct agent_config* config);

#endif
