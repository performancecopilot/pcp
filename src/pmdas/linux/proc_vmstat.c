/*
 * Linux /proc/vmstat metrics cluster
 *
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#ident "$Id: proc_vmstat.c,v 1.2 2006/06/15 08:05:47 makc Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include "pmapi.h"
#include "proc_vmstat.h"

static int started = 0;

static proc_vmstat_t vmstat;

static struct {
    char	*field;
    __uint64_t	*offset;
} vmstat_fields[] = {
    {"nr_dirty",		&vmstat.nr_dirty },
    {"nr_writeback",		&vmstat.nr_writeback },
    {"nr_unstable",		&vmstat.nr_unstable },
    {"nr_page_table_pages",	&vmstat.nr_page_table_pages },
    {"nr_mapped",		&vmstat.nr_mapped },
    {"nr_slab",			&vmstat.nr_slab },
    {"pgpgin",			&vmstat.pgpgin },
    {"pgpgout",			&vmstat.pgpgout },
    {"pswpin",			&vmstat.pswpin },
    {"pswpout",			&vmstat.pswpout },
    {"pgalloc_high",		&vmstat.pgalloc_high },
    {"pgalloc_normal",		&vmstat.pgalloc_normal },
    {"pgalloc_dma",		&vmstat.pgalloc_dma },
    {"pgfree",			&vmstat.pgfree },
    {"pgactivate",		&vmstat.pgactivate },
    {"pgdeactivate",		&vmstat.pgdeactivate },
    {"pgfault",			&vmstat.pgfault },
    {"pgmajfault",		&vmstat.pgmajfault },
    {"pgrefill_high",		&vmstat.pgrefill_high },
    {"pgrefill_normal",		&vmstat.pgrefill_normal },
    {"pgrefill_dma",		&vmstat.pgrefill_dma },
    {"pgsteal_high",		&vmstat.pgsteal_high },
    {"pgsteal_normal",		&vmstat.pgsteal_normal },
    {"pgsteal_dma",		&vmstat.pgsteal_dma },
    {"pgscan_kswapd_high",	&vmstat.pgscan_kswapd_high },
    {"pgscan_kswapd_normal",	&vmstat.pgscan_kswapd_normal },
    {"pgscan_kswapd_dma",	&vmstat.pgscan_kswapd_dma },
    {"pgscan_direct_high",	&vmstat.pgscan_direct_high },
    {"pgscan_direct_normal",	&vmstat.pgscan_direct_normal },
    {"pgscan_direct_dma",	&vmstat.pgscan_direct_dma },
    {"pginodesteal",		&vmstat.pginodesteal },
    {"slabs_scanned",		&vmstat.slabs_scanned },
    {"kswapd_steal",		&vmstat.kswapd_steal },
    {"kswapd_inodesteal",	&vmstat.kswapd_inodesteal },
    {"pageoutrun",		&vmstat.pageoutrun },
    {"allocstall",		&vmstat.allocstall },
    {"pgrotated",		&vmstat.pgrotated },
    { NULL, NULL }
};

#define VMSTAT_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)vmstat_fields[ii].offset - (__psint_t)&vmstat)

int
refresh_proc_vmstat(proc_vmstat_t *proc_vmstat)
{
    char	buf[1024];
    char	*bufp;
    int64_t	*p;
    int		i;
    FILE	*fp;

    if (!started) {
    	started = 1;
	memset(proc_vmstat, 0, sizeof(proc_vmstat));
    }

    for (i=0; vmstat_fields[i].field != NULL; i++) {
	p = VMSTAT_OFFSET(i, proc_vmstat);
	*p = -1; /* marked as "no value available" */
    }

    if ((fp = fopen("/proc/vmstat", "r")) == (FILE *)0) {
    	return -errno;
    }
    _pm_have_proc_vmstat = 1;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((bufp = strchr(buf, ' ')) == NULL)
	    continue;
	*bufp = '\0';
	for (i=0; vmstat_fields[i].field != NULL; i++) {
	    if (strcmp(buf, vmstat_fields[i].field) != 0)
		continue;
	    p = VMSTAT_OFFSET(i, proc_vmstat);
	    for (bufp++; *bufp; bufp++) {
	    	if (isdigit(*bufp)) {
		    sscanf(bufp, "%llu", (unsigned long long *)p);
		    break;
		}
	    }
	}
    }

    fclose(fp);

    /* success */
    return 0;
}
