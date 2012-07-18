/*
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "proc_net_sockstat.h"

int
refresh_proc_net_sockstat(proc_net_sockstat_t *proc_net_sockstat)
{
    static int started;
    char buf[1024];
    char fmt[64];
    FILE *fp;

    if (!started) {
    	started = 1;
	memset(proc_net_sockstat, 0, sizeof(*proc_net_sockstat));
    }

    if ((fp = fopen("/proc/net/sockstat", "r")) == NULL) {
    	return -oserror();
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
    	if (strncmp(buf, "TCP:", 4) == 0) {
	    sscanf(buf, "%s %s %d %s %d", fmt, fmt, 
	    	&proc_net_sockstat->tcp[_PM_SOCKSTAT_INUSE], fmt,
	    	&proc_net_sockstat->tcp[_PM_SOCKSTAT_HIGHEST]);
	    proc_net_sockstat->tcp[_PM_SOCKSTAT_UTIL] = 
	    	proc_net_sockstat->tcp[_PM_SOCKSTAT_HIGHEST] != 0 ? 
	    	100 * proc_net_sockstat->tcp[_PM_SOCKSTAT_INUSE] /
	    	proc_net_sockstat->tcp[_PM_SOCKSTAT_HIGHEST] : 0;
	}
	else
    	if (strncmp(buf, "UDP:", 4) == 0) {
	    sscanf(buf, "%s %s %d %s %d", fmt, fmt, 
	    	&proc_net_sockstat->udp[_PM_SOCKSTAT_INUSE], fmt,
	    	&proc_net_sockstat->udp[_PM_SOCKSTAT_HIGHEST]);
	    proc_net_sockstat->udp[_PM_SOCKSTAT_UTIL] = 
	    	proc_net_sockstat->udp[_PM_SOCKSTAT_HIGHEST] != 0 ? 
	    	100 * proc_net_sockstat->udp[_PM_SOCKSTAT_INUSE] /
	    	proc_net_sockstat->udp[_PM_SOCKSTAT_HIGHEST] : 0;
	}
	else
    	if (strncmp(buf, "RAW:", 4) == 0) {
	    sscanf(buf, "%s %s %d %s %d", fmt, fmt, 
	    	&proc_net_sockstat->raw[_PM_SOCKSTAT_INUSE], fmt,
	    	&proc_net_sockstat->raw[_PM_SOCKSTAT_HIGHEST]);
	    proc_net_sockstat->raw[_PM_SOCKSTAT_UTIL] = 
	    	proc_net_sockstat->raw[_PM_SOCKSTAT_HIGHEST] != 0 ? 
	    	100 * proc_net_sockstat->raw[_PM_SOCKSTAT_INUSE] /
	    	proc_net_sockstat->raw[_PM_SOCKSTAT_HIGHEST] : 0;
	}
    }

    fclose(fp);

    /* success */
    return 0;
}

