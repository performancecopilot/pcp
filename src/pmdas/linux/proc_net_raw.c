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
#include "proc_net_raw.h"

static int
refresh_rawconn_stats(rawconn_stats_t *conn, const char *path)
{
    char		buf[BUFSIZ]; 
    FILE		*fp;

    memset(conn, 0, sizeof(*conn));

    if ((fp = linux_statsfile(path, buf, sizeof(buf))) == NULL)
	return -oserror();

    /* skip header, then ... */
    if (fgets(buf, sizeof(buf), fp) != NULL) {
	/* ... accumlate count of the remaining lines */
	while (fgets(buf, sizeof(buf), fp) != NULL)
	    conn->count++;
    }

    fclose(fp);
    return 0;
}

int
refresh_proc_net_raw(proc_net_raw_t *proc_net_raw)
{
    return refresh_rawconn_stats(proc_net_raw, "/proc/net/raw");
}

int
refresh_proc_net_raw6(proc_net_raw6_t *proc_net_raw6)
{
    return refresh_rawconn_stats(proc_net_raw6, "/proc/net/raw6");
}
