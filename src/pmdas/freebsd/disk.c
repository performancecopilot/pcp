/*
 * FreeBSD Kernel PMDA - disk metrics
 *
 * Copyright (c) 2012 Ken McDonell.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <devstat.h>
#include "freebsd.h"

struct devinfo	devinfo = { 0 };
struct statinfo	statinfo;

void
refresh_disk_metrics(void)
{
    static int	init_done = 0;
    int		i;
    int		sts;

    if (!init_done) {
	sts = devstat_checkversion(NULL);
	if (sts != 0) {
	    fprintf(stderr, "refresh_disk_metrics: devstat_checkversion: failed! %s\n", devstat_errbuf);
	    exit(1);
	}
	statinfo.dinfo = &devinfo;
	init_done = 1;
    }

    sts = devstat_getdevs(NULL, &statinfo);
    if (sts < 0) {
	fprintf(stderr, "refresh_disk_metrics: devstat_getdevs: %s\n", strerror(errno));
	exit(1);
    }
    else if (sts == 1) {
	/*
	 * First call, else devstat[] list has changed
	 */
	struct devstat	*dsp;
	char		iname[DEVSTAT_NAME_LEN+6];
	pmdaCacheOp(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_INACTIVE);
	for (i = 0; i < devinfo.numdevs; i++) {
	    dsp = &devinfo.devices[i];
	    /*
	     * Skip entries that are not interesting ... only include
	     * "da" (direct access) disks at this stage
	     */
	    if (strcmp(dsp->device_name, "da") != 0)
		continue;
	    pmsprintf(iname, sizeof(iname), "%s%d", dsp->device_name, dsp->unit_number);
	    sts = pmdaCacheLookupName(indomtab[DISK_INDOM].it_indom, iname, NULL, NULL);
	    if (sts == PMDA_CACHE_ACTIVE) {
		int	j;
		fprintf(stderr, "refresh_disk_metrics: Warning: duplicate name (%s) in disk indom\n", iname);
		for (j = 0; j < devinfo.numdevs; j++) {
		    dsp = &devinfo.devices[j];
		    fprintf(stderr, "  devinfo[%d]: %s%d\n", j, dsp->device_name, dsp->unit_number);
		}
		continue;
	    }
	    else {
		/* new entry or reactivate an existing one */
		pmdaCacheStore(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_ADD, iname, (void *)dsp);
	    }
	}
    }

}

int
do_disk_metrics(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    struct devstat	*dsp;
    int			sts;

    if (inst != PM_IN_NULL) {
	/*
	 * per-disk metrics
	 */
	sts = pmdaCacheLookup(indomtab[DISK_INDOM].it_indom, inst, NULL, (void **)&dsp);
	if (sts == PMDA_CACHE_ACTIVE) {
	    sts = 1;
	    /* cluster and domain already checked, just need item ... */
	    switch (pmid_item(mdesc->m_desc.pmid)) {
		case 0:		/* disk.dev.read */
		    atom->ull = dsp->operations[DEVSTAT_READ];
		    break;

		case 1:		/* disk.dev.write */
		    atom->ull = dsp->operations[DEVSTAT_WRITE];
		    break;

		case 2:		/* disk.dev.total */
		    atom->ull = dsp->operations[DEVSTAT_READ] + dsp->operations[DEVSTAT_WRITE];
		    break;

		case 3:		/* disk.dev.read_bytes */
		    atom->ull = dsp->bytes[DEVSTAT_READ];
		    break;

		case 4:		/* disk.dev.write_bytes */
		    atom->ull = dsp->bytes[DEVSTAT_WRITE];
		    break;

		case 5:		/* disk.dev.total_bytes */
		    atom->ull = dsp->bytes[DEVSTAT_READ] + dsp->bytes[DEVSTAT_WRITE];
		    break;

		case 12:	/* disk.dev.blkread */
		    atom->ull = dsp->block_size == 0 ? 0 : dsp->bytes[DEVSTAT_READ] / dsp->block_size;
		    break;

		case 13:	/* disk.dev.blkwrite */
		    atom->ull = dsp->block_size == 0 ? 0 : dsp->bytes[DEVSTAT_WRITE] / dsp->block_size;
		    break;

		case 14:	/* disk.dev.blktotal */
		    atom->ull = dsp->block_size == 0 ? 0 : (dsp->bytes[DEVSTAT_READ] + dsp->bytes[DEVSTAT_WRITE]) / dsp->block_size;
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
	int	i;
	atom->ull = 0;
	sts = 1;
	pmdaCacheOp(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_WALK_REWIND);
	while (sts == 1 && (i = pmdaCacheOp(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_WALK_NEXT)) >= 0) {
	    int		lsts;
	    lsts = pmdaCacheLookup(indomtab[DISK_INDOM].it_indom, i, NULL, (void **)&dsp);
	    if (lsts == PMDA_CACHE_ACTIVE) {
		/* cluster and domain already checked, just need item ... */
		switch (pmid_item(mdesc->m_desc.pmid)) {
		    case 6:		/* disk.all.read */
			atom->ull += dsp->operations[DEVSTAT_READ];
			break;

		    case 7:		/* disk.all.write */
			atom->ull += dsp->operations[DEVSTAT_WRITE];
			break;

		    case 8:		/* disk.all.total */
			atom->ull += dsp->operations[DEVSTAT_READ] + dsp->operations[DEVSTAT_WRITE];
			break;

		    case 9:		/* disk.all.read_bytes */
			atom->ull += dsp->bytes[DEVSTAT_READ];
			break;

		    case 10:		/* disk.all.write_bytes */
			atom->ull += dsp->bytes[DEVSTAT_WRITE];
			break;

		    case 11:		/* disk.all.total_bytes */
			atom->ull += dsp->bytes[DEVSTAT_READ] + dsp->bytes[DEVSTAT_WRITE];
			break;

		    case 15:		/* disk.all.blkread */
			atom->ull += dsp->block_size == 0 ? 0 : dsp->bytes[DEVSTAT_READ] / dsp->block_size;
			break;

		    case 16:		/* disk.all.blkwrite */
			atom->ull += dsp->block_size == 0 ? 0 : dsp->bytes[DEVSTAT_WRITE] / dsp->block_size;
			break;

		    case 17:		/* disk.all.blktotal */
			atom->ull += dsp->block_size == 0 ? 0 : (dsp->bytes[DEVSTAT_READ] + dsp->bytes[DEVSTAT_WRITE]) / dsp->block_size;
			break;

		    default:
			sts = PM_ERR_PMID;
			break;
		}
	    }
	}
	if (i < 0 && i != -1)
	    /* not end of indom from cache walk, some other error */
	    sts = i;
    }

    return sts;
}
