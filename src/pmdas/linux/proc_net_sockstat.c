/*
 * Copyright (c) 2014-2017 Red Hat.
 * Copyright (c) 2017 Fujitsu.
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
#include "linux.h"
#include "proc_net_sockstat.h"

int
refresh_proc_net_sockstat(proc_net_sockstat_t *proc_net_sockstat)
{
    char buf[1024];
    char fmt[64];
    FILE *fp;

    if ((fp = linux_statsfile("/proc/net/sockstat", buf, sizeof(buf))) == NULL)
	return -oserror();

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (strncmp(buf, "sockets:", 8) == 0) {
	    sscanf(buf, "%s %s %d", fmt, fmt,
	    	&proc_net_sockstat->total);
        } 
	else if (strncmp(buf, "TCP:", 4) == 0) {
	    sscanf(buf, "%s %s %d %s %d %s %d %s %d %s %d",
                fmt, fmt,
	    	&proc_net_sockstat->tcp_inuse, fmt,
	    	&proc_net_sockstat->tcp_orphan, fmt,
	    	&proc_net_sockstat->tcp_tw, fmt,
	    	&proc_net_sockstat->tcp_alloc, fmt,
	    	&proc_net_sockstat->tcp_mem);
        } 
	else
	if (strncmp(buf, "UDP:", 4) == 0) {
	    sscanf(buf, "%s %s %d %s %d", fmt, fmt, 
	    	&proc_net_sockstat->udp_inuse, fmt,
	    	&proc_net_sockstat->udp_mem);
	}
	else
	if (strncmp(buf, "UDPLITE:", 8) == 0) {
	    sscanf(buf, "%s %s %d", fmt, fmt, 
	    	&proc_net_sockstat->udplite_inuse);
        }
	else
	if (strncmp(buf, "RAW:", 4) == 0) {
	    sscanf(buf, "%s %s %d", fmt, fmt, 
	    	&proc_net_sockstat->raw_inuse);
	}
	else
	if (strncmp(buf, "FRAG:", 5) == 0) {
	    sscanf(buf, "%s %s %d %s %d", fmt, fmt, 
	    	&proc_net_sockstat->frag_inuse, fmt,
	    	&proc_net_sockstat->frag_memory);
	}
    }

    fclose(fp);
    return 0;
}
