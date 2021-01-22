/*
 * Copyright (c) 2021 Red Hat.
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

#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include "ss_stats.h"

#define SS_OPTIONS "-noemitauOH"

/* default filter: reduces logging overheads */
#define SS_DEFAULT_FILTER "state connected"

char *ss_filter = NULL; /* storable: network.persocket.filter */

FILE *
ss_open_stream()
{
    FILE *fp;
    char *path;
    char cmd[MAXPATHLEN];

    if (ss_filter == NULL) {
	/* pmstore to network.persocket.filter frees this if changing */
    	if ((ss_filter = strdup(SS_DEFAULT_FILTER)) == NULL)
	    return NULL;
    }

    if ((path = getenv("PCPQA_PMDA_SOCKETS")) != NULL)
	/* PCPQA input file */
    	fp = fopen(path, "r");
    else {
	/* TODO use a config file for ss options and ss filters */
	if (access((path = "/usr/sbin/ss"), X_OK) != 0) {
	    if (access((path = "/usr/bin/ss"), X_OK) != 0) {
	    	fprintf(stderr, "Error: no \"ss\" binary found\n");
		return NULL;
	    }
	}
	pmsprintf(cmd, sizeof(cmd), "%s %s %s", path, SS_OPTIONS, ss_filter);
	fp = popen(cmd, "r");
    }

    return fp;
}

void
ss_close_stream(FILE *fp)
{
    if (getenv("PCPQA_PMDA_SOCKETS") != NULL)
    	fclose(fp);
    else
    	pclose(fp);
}
