/*
 * Copyright (c) 2017 Fujitsu.
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
#include "proc_net_sockstat6.h"

int
refresh_proc_net_sockstat6(proc_net_sockstat6_t *proc_net_sockstat6)
{
    char buf[1024];
    char fmt[64];
    FILE *fp;

    if ((fp = linux_statsfile("/proc/net/sockstat6", buf, sizeof(buf))) == NULL)
	return -oserror();

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (strncmp(buf, "TCP6:", 5) == 0) {
	    sscanf(buf, "%s %s %d", fmt, fmt,
	    	&proc_net_sockstat6->tcp6_inuse);
        } 
	else
	if (strncmp(buf, "UDP6:", 5) == 0) {
	    sscanf(buf, "%s %s %d", fmt, fmt, 
	    	&proc_net_sockstat6->udp6_inuse);
	}
	else
	if (strncmp(buf, "UDPLITE6:", 9) == 0) {
	    sscanf(buf, "%s %s %d", fmt, fmt, 
	    	&proc_net_sockstat6->udplite6_inuse);
        }
	else
	if (strncmp(buf, "RAW6:", 5) == 0) {
	    sscanf(buf, "%s %s %d", fmt, fmt, 
	    	&proc_net_sockstat6->raw6_inuse);
	}
	if (strncmp(buf, "FRAG6:", 6) == 0) {
	    sscanf(buf, "%s %s %d %s %d", fmt, fmt, 
	    	&proc_net_sockstat6->frag6_inuse, fmt,
	    	&proc_net_sockstat6->frag6_memory);
	}
    }

    fclose(fp);
    return 0;
}
