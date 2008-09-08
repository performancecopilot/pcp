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

#ident "$Id: proc_net_tcp.c,v 1.7 2008/05/15 04:54:16 kimbrr Exp $"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>

#include "proc_net_tcp.h"

#define MYBUFSZ (1<<14) /*16k*/

int
refresh_proc_net_tcp(proc_net_tcp_t *proc_net_tcp)
{
    FILE *fp;
    char buf[MYBUFSZ]; 
    char *p = buf;
    char *q;
    unsigned int n;
    ssize_t got = 0;
    ptrdiff_t remnant= 0;

    memset(proc_net_tcp, 0, sizeof(*proc_net_tcp));

    if ((fp = fopen("/proc/net/tcp", "r")) == NULL) {
    	return -errno;
    }
    fgets(buf, sizeof(buf)-1, fp); /* skip header */
    for (buf[0]='\0';;) {
	q = strchrnul(p, '\n');
	if (*q == '\n') {
	    if (1 == sscanf(p, " %*s %*s %*s %x", &n)
		&& n < _PM_TCP_LAST) {
		proc_net_tcp->stat[n]++;
            }
	    p = q + 1;
	    continue;
	}
	remnant = (q - p);
	if (remnant > 0 && p != buf) 
	    memmove(buf, p, remnant);

	got = read(fileno(fp), buf + remnant, MYBUFSZ - remnant - 1);
	if (got <= 0)
	    break;

	buf[remnant + got] = '\0';
	p = buf;
    }

    fclose(fp);

    /* success */
    return 0;
}

