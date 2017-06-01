/*
 * Initializes maximum memory bandwidth per numa node.
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
#include <ctype.h>
#include <sys/stat.h>
#include "linux.h"
#include "proc_cpuinfo.h"
#include "proc_stat.h"
#include "numa_meminfo.h"

#define VERSION_STR "Version"
#define SUPP_VERSION "1.0"
#define MAX_NAME_LEN 512

/*
 * Check whether bandwidth.conf has changed
 */
int bandwidth_conf_changed(char *conf_path)
{
    struct stat stat_buf;
    static time_t last_errno;
    static time_t last_mtime;

    if (stat(conf_path, &stat_buf) != 0) {
	if (errno != last_errno) {
	    if (errno != ENOENT)
		fprintf(stderr, "Cannot stat %s\n", conf_path);
	    last_errno = errno;
	    return 1;
	}
	return 0;
    }
    last_errno = 0;

    if (stat_buf.st_mtime != last_mtime) {
	last_mtime = stat_buf.st_mtime;
	return 1;
    }

    return 0;
}

static void skim_through_whitespace(char *start_ptr, char *end_ptr)
{
    while ((start_ptr != end_ptr) && isspace(*start_ptr))
	start_ptr++;
}

static int validate_conf_version(const char *conf, char *start, char *end)
{
    char *ptr;

    ptr = strchr(start, ':');
    if (!ptr) {
	fprintf(stderr, "Version information missing in %s\n", conf);
	return -1;
    }
    *ptr = '\0';
    ptr++;
    skim_through_whitespace(ptr, end);
    if (!strncmp(start, VERSION_STR, strlen(VERSION_STR)) &&
	!(strncmp(ptr, SUPP_VERSION, strlen(SUPP_VERSION))))
	return 0;
    fprintf(stderr, "Unsupported %s version '%s', current version: %s\n",
	    conf, ptr, SUPP_VERSION);
    return -1;
}

int get_memory_bandwidth_conf(char *config)
{
    size_t len = 0;
    char *start_ptr, *end_ptr, *value_str, *line = NULL;
    FILE *fp;
    pmInDom nodes;
    pernode_t *np;
    ssize_t ret = 0;
    char *node_name;
    int nodes_found = 0;
    int version_found = 0;

    if ((fp = fopen(config, "r")) == NULL)
	return 0;

    nodes = INDOM(NODE_INDOM);

    while (ret >= 0) {
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
		ret = validate_conf_version(config, start_ptr, end_ptr);
		if (ret < 0) {
		    goto free_line;
		} else {
		    version_found = 1;
		    continue;
		}
	    }

	    if (!version_found) {
		ret = -1;
		fprintf(stderr, "Version missing at the start of %s\n", config);
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

	    np = NULL;
	    if (!pmdaCacheLookupName(nodes, node_name, NULL, (void **)&np) || !np) {
		fprintf(stderr, "Unknown node '%s' in %s\n", node_name, config);
		ret = -1;
		goto free_line;
	    }
	    np->bandwidth = atof(value_str);
	    nodes_found++;
	}
    }

    if (nodes_found == pmdaCacheOp(nodes, PMDA_CACHE_SIZE))
	ret = 0;

 free_line:
    if (line)
	free(line);
    fclose(fp);

    return ret;
}
