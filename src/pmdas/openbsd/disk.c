/*
 * OpenBSD Kernel PMDA - disk metrics
 *
 * Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * TODO
 *   +  time_sec and time_usec from diskstats seems to be device
 *	busy time, so a possible source for disk.dev.avactive
 */

#include "pmapi.h"
#include "pmda.h"
#include "openbsd.h"
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/disk.h>
#include <errno.h>
#include <string.h>

static int		ndisk = -1;
static struct diskstats	*stats;
static int		valid;

void
refresh_disk_metrics(void)
{
    int		sts;
    static int	name[] = { CTL_HW, HW_DISKSTATS };
    u_int	namelen = sizeof(name) / sizeof(name[0]);
    size_t	buflen;
    int		i;

    /* get number of diskstats structs available */
    if ((sts = sysctl(name, namelen, NULL, &buflen, NULL, 0)) != 0) {
	fprintf(stderr, "refresh_disk_metrics: ndisk sysctl(): %s\n", strerror(errno));
	valid = 0;
	return;
    }

    if (ndisk != buflen / sizeof(struct diskstats)) {
	/* initialization or something has changed */
	if (ndisk == -1) {
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "Info: refresh_disk_metrics: initial ndisk=%d\n", (int)(buflen / sizeof(struct diskstats)));
	    pmdaCacheOp(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_LOAD);
	}
	else {
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "Info: refresh_disk_metrics: ndisk changed from %d to %d\n", ndisk, (int)(buflen / sizeof(struct diskstats)));
	    pmdaCacheOp(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_INACTIVE);
	}
	ndisk = buflen / sizeof(struct diskstats);
	if (stats != NULL)
	    free(stats);
	stats = (struct diskstats *)malloc(buflen);
	if (stats == NULL) {
	    pmNoMem("refresh_disk_metrics: stats", buflen, PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	/* fetch all the available data */
	if ((sts = sysctl(name, namelen, stats, &buflen, NULL, 0)) != 0) {
	    fprintf(stderr, "refresh_disk_metrics: stats sysctl(): %s\n", strerror(errno));
	    valid = 0;
	    return;
	}
	for (i = 0; i < ndisk; i++) {
	    if ((sts = pmdaCacheStore(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_ADD, stats[i].ds_name, (void **)&stats[i])) < 0) {
		fprintf(stderr, "refresh_disk_metrics: pmdaCacheStore(%s) failed: %s\n", stats[i].ds_name, pmErrStr(sts));
		continue;
	    }
	}
	pmdaCacheOp(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_SAVE);
    }
    else {
	/*
	 * no change in the number of diskstats structs avaiable,
	 * assume the order remains the same, so just get 'em
	 */
	if ((sts = sysctl(name, namelen, stats, &buflen, NULL, 0)) != 0) {
	    fprintf(stderr, "refresh_disk_metrics: stats sysctl(): %s\n", strerror(errno));
	    valid = 0;
	    return;
	}
    }

    valid = 1;
}

int
do_disk_metrics(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    int			sts;
    int			l_inst;
    struct diskstats	*sp;

    if (!valid)
	return 0;

    if (inst != PM_IN_NULL) {
	/*
	 * per-disk metrics
	 */
	sts = pmdaCacheLookup(indomtab[DISK_INDOM].it_indom, inst, NULL, (void **)&sp);
	if (sts == PMDA_CACHE_ACTIVE) {
	    sts = 1;
	    /* cluster and domain already checked, just need item ... */
	    switch (pmID_item(mdesc->m_desc.pmid)) {
		case 0:		/* disk.dev.read */
		    atom->ull = sp->ds_rxfer;
		    break;

		case 1:		/* disk.dev.write */
		    atom->ull = sp->ds_wxfer;
		    break;

		case 2:		/* disk.dev.total */
		    atom->ull = sp->ds_rxfer + sp->ds_wxfer;
		    break;

		case 3:		/* disk.dev.read_bytes */
		    atom->ull = sp->ds_rbytes;
		    break;

		case 4:		/* disk.dev.write_bytes */
		    atom->ull = sp->ds_wbytes;
		    break;

		case 5:		/* disk.dev.total_bytes */
		    atom->ull = sp->ds_rbytes + sp->ds_wbytes;
		    break;

		default:
		    sts = PM_ERR_PMID;
		    break;
	    }
	}
	else
	    sts = 0;
    }
    else {
	/*
	 * all-disk summary metrics
	 */
	atom->ull = 0;
	sts = 1;
	for (pmdaCacheOp(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((l_inst = pmdaCacheOp(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_WALK_NEXT)) < 0)
		break;
	    if (!pmdaCacheLookup(indomtab[DISK_INDOM].it_indom, l_inst, NULL, (void **)&sp))
		continue;
	    /* cluster and domain already checked, just need item ... */
	    switch (pmID_item(mdesc->m_desc.pmid)) {
		case 6:		/* disk.all.read */
		    atom->ull += sp->ds_rxfer;
		    break;

		case 7:		/* disk.all.write */
		    atom->ull += sp->ds_wxfer;
		    break;

		case 8:		/* disk.all.total */
		    atom->ull += sp->ds_rxfer + sp->ds_wxfer;
		    break;

		case 9:		/* disk.all.read_bytes */
		    atom->ull += sp->ds_rbytes;
		    break;

		case 10:	/* disk.all.write_bytes */
		    atom->ull += sp->ds_wbytes;
		    break;

		case 11:	/* disk.all.total_bytes */
		    atom->ull += sp->ds_rbytes + sp->ds_wbytes;
		    break;

		default:
		    sts = PM_ERR_PMID;
		    break;
	    }
	}
    }

    return sts;
}
