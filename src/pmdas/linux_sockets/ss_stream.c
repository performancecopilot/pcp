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

FILE *
ss_open_stream()
{
    FILE *fp;
    char *path;

    if ((path = getenv("PCPQA_PMDA_SOCKETS")) != NULL)
	/* PCPQA input file */
    	fp = fopen(path, "r");
    else {
	/* TODO use a config file for ss options and ss filters */
	if (access("/usr/sbin/ss", X_OK) == 0)
	    fp = popen("/usr/sbin/ss -noemitauOH", "r"); /* Fedora/RHEL */
	else if (access("/usr/bin/ss", X_OK) == 0)
	    fp = popen("/usr/bin/ss -noemitauOH", "r"); /* Ubuntu */
	else
	    fp = NULL;
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
