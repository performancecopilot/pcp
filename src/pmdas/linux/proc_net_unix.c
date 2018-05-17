/*
 * Copyright (c) 2018 Red Hat.
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
#include "proc_net_unix.h"

int
refresh_proc_net_unix(proc_net_unix_t *up)
{
    char		buf[BUFSIZ]; 
    char		*q, *p = buf;
    FILE		*fp;
    ssize_t		got = 0;
    ptrdiff_t		remnant = 0;
    unsigned int	type, state;

    memset(up, 0, sizeof(*up));

    if ((fp = linux_statsfile("/proc/net/unix", buf, sizeof(buf))) == NULL)
	return -oserror();

    for (buf[0]='\0';;) {
	q = strchrnul(p, '\n');
	if (*q == '\n') {
	    if (sscanf(p, "%*s %*s %*s %*s %*s %x %x", &type, &state) == 2) {
		if (type == 0x0002)
		    up->datagram_count++;
		else if (type == 0x0001) {
		    if (state == 0x01)
			up->stream_listen++;
		    else if (state == 0x03)
			up->stream_established++;
		    up->stream_count++;
		}
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
