/* Initializes the maximum memory bandwidth per numa node
 *
 * Copyright (c) 2016 Hemant K. Shaw, IBM Corporation.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include "proc_cpuinfo.h"
#include "proc_stat.h"
#include "numa_meminfo.h"

#define VERSION_STR "Version"
#define SUPP_VERSION "1.0"
#define MAX_NAME_LEN 512

static void skim_through_whitespace(char *start_ptr, char *end_ptr)
{
    while((start_ptr != end_ptr) && isspace(*start_ptr))
	start_ptr++;
}

static int find_node_match(char *name, int nr_nodes)
{
    int i;
    char node_name[MAX_NAME_LEN];

    for (i = 0; i < nr_nodes; i++) {
	snprintf(node_name, MAX_NAME_LEN, "%s%d", "node", i);
	if (!strncmp(node_name, name, strlen(name)))
	    return i;
    }
    return -1;
}

static int validate_conf_version(char *start, char *end)
{
    char *ptr;

    ptr = strchr(start, ':');
    if (!ptr) {
	fprintf(stderr, "Version information missing in bandwidth.conf");
	return -1;
    }
    *ptr = '\0';
    ptr++;
    skim_through_whitespace(ptr, end);
    if (!strncmp(start, VERSION_STR, strlen(VERSION_STR)) &&
	!(strncmp(ptr, SUPP_VERSION, strlen(SUPP_VERSION))))
	return 0;
    fprintf(stderr, "Unsupported bandwidth.conf version, expected version : %s",
	    SUPP_VERSION);
    return -1;
}

int get_memory_bandwidth_conf(numa_meminfo_t *numa_meminfo, int nr_nodes)
{
    size_t len = 0;
    char *start_ptr, *end_ptr, *value_str, *line = NULL;
    FILE *fp;
    ssize_t ret = 0;
    char *node_name;
    int nodes_found = 0, id;
    int version_found = 0;

    fp = fopen(numa_meminfo->bandwidth_conf, "r");
    if (NULL == fp) {
	fprintf(stderr, "Error in opening %s\n", numa_meminfo->bandwidth_conf);
	return -1;
    }

    while(ret >= 0) {
	ret = getline(&line, &len, fp);
	if (ret > 0) {
	    /* Ignore the comments */
	    if (line[0] == '#') {
		continue;
	    }
	    /* Remove the new line from the end of the string here (if any) */
	    if (line[strlen(line) - 1] == '\n')
		line[strlen(line) - 1] = '\0';

	    start_ptr = line;
	    end_ptr = start_ptr + strlen(line) - 1;

	    /* Ignore white-space */
	    skim_through_whitespace(start_ptr, end_ptr);

	    /* Verify the version information */
	    if (strstr(start_ptr, VERSION_STR)) {
		ret = validate_conf_version(start_ptr, end_ptr);
		if (ret < 0) {
		    goto free_line;
		} else {
		    version_found = 1;
		    continue;
		}
	    }

	    if (!version_found) {
		ret = -1;
		fprintf(stderr, "Version needs to be specified at the beginning of bandwidth.conf file\n");
		goto free_line;
	    }

	    value_str = strchr(line, ':');
	    if (NULL == value_str) {
		ret = -1;
		goto free_line;
	    }

	    *value_str = '\0';
	    value_str++;

	    node_name = start_ptr;

	    id = find_node_match(node_name, nr_nodes);
	    if (id == -1) {
		fprintf(stderr, "Unknown node name provided in bandwidth.conf\n");
		return -1;
	    }
	    numa_meminfo->node_info[id].bandwidth = atof(value_str);
	    nodes_found++;
	}
    }

    if (nodes_found == nr_nodes)
	ret = 0;

 free_line:
    if (line)
	free(line);
    fclose(fp);

    return ret;
}
