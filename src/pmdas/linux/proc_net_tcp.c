/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1999,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * This code contributed by Michal Kara (lemming@arthur.plbohnice.cz)
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
#include "linux.h"
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
    ptrdiff_t remnant = 0;

    memset(proc_net_tcp, 0, sizeof(*proc_net_tcp));

    if ((fp = linux_statsfile("/proc/net/tcp", buf, sizeof(buf))) == NULL)
	return -oserror();

    /* skip header */
    if (fgets(buf, sizeof(buf), fp) == NULL) {
    	/* oops, no header! */
	fclose(fp);
	return -oserror();
    }
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
    return 0;
}
