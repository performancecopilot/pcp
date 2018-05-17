/*
 * Copyright (c) 2014,2018 Red Hat.
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
#include "proc_net_udp.h"

static int
refresh_udpconn_stats(udpconn_stats_t *conn, const char *path)
{
    char		buf[BUFSIZ]; 
    char		*q, *p = buf;
    FILE		*fp;
    ssize_t		got = 0;
    ptrdiff_t		remnant = 0;
    unsigned int	n;

    memset(conn, 0, sizeof(*conn));

    if ((fp = linux_statsfile(path, buf, sizeof(buf))) == NULL)
	return -oserror();

    for (buf[0]='\0';;) {
	q = strchrnul(p, '\n');
	if (*q == '\n') {
	    if (sscanf(p, " %*s %*s %*s %x", &n) == 1) {
		if (n == 0x07)
		    conn->listen++;
		if (n == 0x01)
		    conn->established++;
	    }
	    p = q + 1;
	    continue;
	}
	remnant = (q - p);
	if (remnant > 0 && p != buf) 
	    memmove(buf, p, remnant);

	got = read(fileno(fp), buf + remnant, BUFSIZ - remnant - 1);
	if (got <= 0)
	    break;

	buf[remnant + got] = '\0';
	p = buf;
    }

    fclose(fp);
    return 0;
}

int
refresh_proc_net_udp(proc_net_udp_t *proc_net_udp)
{
    return refresh_udpconn_stats(proc_net_udp, "/proc/net/udp");
}

int
refresh_proc_net_udp6(proc_net_udp6_t *proc_net_udp6)
{
    return refresh_udpconn_stats(proc_net_udp6, "/proc/net/udp6");
}
