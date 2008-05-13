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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "proc_net_snmp.h"

static int started = 0;

static void
get_fields(unsigned int *fields, char *buf, int n)
{
    int i;
    char *p = strtok(buf, " ");
    for (i=0; i < n; i++) {
	/* skip the first field (heading) */
	if ((p = strtok(NULL, " ")) != NULL)
	    fields[i] = strtoul(p, NULL, 0);
	else
	    fields[i] = 0;
    }
}

int
refresh_proc_net_snmp(proc_net_snmp_t *proc_net_snmp)
{
    char buf[1024];
    FILE *fp;

    if (!started) {
	started = 1;
	memset(proc_net_snmp, 0, sizeof(proc_net_snmp));
    }

    if ((fp = fopen("/proc/net/snmp", "r")) == NULL)
	return -errno;

    /*
    * This is really bogus.
    * We are consuming the heading line in the while (fgets(..)) and
    * discarding it, then reading the next line and parsing the counters
    * from there, with hard coded semantics as to the meaning of each
    * counter found on this line.
    * This does not work at all for IcmpMsg where the line we're
    * ignoring provides the name of the counters on the line we're
    * scanning (pairs appear to be like this, we're just lucky that
    * the names in the ignored lines are not changing ... of course
    * when they change we'll export complete nonsense.
    * - Ken
    */
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (fgets(buf, sizeof(buf), fp) != NULL) {
	    if (strncmp(buf, "Ip:", 3) == 0)
		get_fields(proc_net_snmp->ip, buf, _PM_SNMP_IP_NFIELDS);
	    else
	    if (strncmp(buf, "Icmp:", 5) == 0)
		get_fields(proc_net_snmp->icmp, buf, _PM_SNMP_ICMP_NFIELDS);
	    else
	    if (strncmp(buf, "Tcp:", 4) == 0)
		get_fields(proc_net_snmp->tcp, buf, _PM_SNMP_TCP_NFIELDS);
	    else
	    if (strncmp(buf, "Udp:", 4) == 0)
		get_fields(proc_net_snmp->udp, buf, _PM_SNMP_UDP_NFIELDS);
	    else
	    if (strncmp(buf, "UdpLite:", 8) == 0)
		get_fields(proc_net_snmp->udplite, buf, _PM_SNMP_UDPLITE_NFIELDS);
	    else
	    if (strncmp(buf, "IcmpMsg:", 8) == 0) {
		/* TODO - no support at this stage, see comment above */
		;
	    }
	    else
	    	fprintf(stderr, "Error: /proc/net/snmp fetch failed: buf: %s",
			buf);
	}
    }

    fclose(fp);

    /* success */
    return 0;
}

