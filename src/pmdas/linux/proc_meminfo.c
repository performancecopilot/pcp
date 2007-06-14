/*
 * Linux /proc/meminfo metrics cluster
 *
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#ident "$Id: proc_meminfo.c,v 1.8 2004/12/15 06:50:50 markgw Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include "pmapi.h"
#include "proc_meminfo.h"

static int started = 0;

static proc_meminfo_t moff;

static struct {
    char	*field;
    int64_t	*offset;
} meminfo_fields[] = {
    { "MemTotal",	&moff.MemTotal },
    { "MemFree",	&moff.MemFree },
    { "MemShared",	&moff.MemShared },
    { "Buffers",	&moff.Buffers },
    { "Cached",		&moff.Cached },
    { "SwapCached",	&moff.SwapCached },
    { "Active",		&moff.Active },
    { "Inactive",	&moff.Inactive },
    { "HighTotal",	&moff.HighTotal },
    { "HighFree",	&moff.HighFree },
    { "LowTotal",	&moff.LowTotal },
    { "LowFree",	&moff.LowFree },
    { "SwapTotal",	&moff.SwapTotal },
    { "SwapFree",	&moff.SwapFree },
    { "Dirty",		&moff.Dirty },
    { "Writeback",	&moff.Writeback },
    { "Mapped",		&moff.Mapped },
    { "Slab",		&moff.Slab },
    { "Committed_AS",	&moff.Committed_AS },
    { "PageTables",	&moff.PageTables },
    { "ReverseMaps",	&moff.ReverseMaps },
    { NULL, NULL }
};

#define MOFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)meminfo_fields[ii].offset - (__psint_t)&moff)

int
refresh_proc_meminfo(proc_meminfo_t *proc_meminfo)
{
    char	buf[1024];
    char	*bufp;
    int64_t	*p;
    int		i;
    FILE	*fp;

    if (!started) {
    	started = 1;
	memset(proc_meminfo, 0, sizeof(proc_meminfo));
    }

    for (i=0; meminfo_fields[i].field != NULL; i++) {
	p = MOFFSET(i, proc_meminfo);
	*p = -1; /* marked as "no value available" */
    }

    if ((fp = fopen("/proc/meminfo", "r")) == (FILE *)0) {
    	return -errno;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((bufp = strchr(buf, ':')) == NULL)
	    continue;
	*bufp = '\0';
	for (i=0; meminfo_fields[i].field != NULL; i++) {
	    if (strcmp(buf, meminfo_fields[i].field) != 0)
		continue;
	    p = MOFFSET(i, proc_meminfo);
	    for (bufp++; *bufp; bufp++) {
	    	if (isdigit(*bufp)) {
		    sscanf(bufp, "%llu", (unsigned long long *)p);
		    *p *= 1024; /* kbytes -> bytes */
		    break;
		}
	    }
	}
    }

    fclose(fp);

    /* success */
    return 0;
}
