/*
 * Copyright (c) 1999,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

/*
 * This code contributed by Michal Kara (lemming@arthur.plbohnice.cz)
 */

#ident "$Id: proc_net_tcp.c,v 1.6 2004/06/24 06:15:36 kenmcd Exp $"

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include "proc_net_tcp.h"

int
refresh_proc_net_tcp(proc_net_tcp_t *proc_net_tcp)
{
    char buf[1024];
    char *s;
    FILE *fp;
    int n;

    memset(proc_net_tcp, 0, sizeof(*proc_net_tcp));

    if ((fp = fopen("/proc/net/tcp", "r")) == NULL) {
    	return -errno;
    }

    while (fgets(buf, sizeof(buf)-1, fp) != NULL) {
	if (!buf[0]) break;
    	buf[sizeof(buf)-1] = 0;
	// Find colon
	s = buf;
	while(*s && (*s != ':')) s++;
	if (*s) {
	    // Skip three spaces
	    n = 3;
	    while(*s && n) {
		if (*s == ' ') n--;
		s++;
	    }
	    if (*s) {
		// Get state
		n = 0;
		for(;;) {
	 	    if (isalpha(*s)) n = (n<<4) + (toupper(*s)-'A'+10);
		    else if (isdigit(*s)) n = (n<<4) + (*s-'0');
		    else break;
		    s++;
	 	}
		if (n < _PM_TCP_LAST) proc_net_tcp->stat[n]++;
	    }
	}
    }

    fclose(fp);

    /* success */
    return 0;
}

