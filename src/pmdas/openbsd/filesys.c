/*
 * OpenBSD Kernel PMDA - filesys metrics
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "openbsd.h"
#include <sys/param.h>
#include <sys/mount.h>
#include <errno.h>
#include <string.h>

static int		nfilesys = -1;
static int		skipped_filesys = 0;
static struct statfs	*stats;
static int		valid;

void
refresh_filesys_metrics(void)
{
    int		sts;
    int		i;

    /* get number of statfs structs available */
    if ((sts = getfsstat(NULL, 0, MNT_NOWAIT)) < 0) {
	fprintf(stderr, "refresh_filesys_metrics: nfilesys getfsstat(): %s\n", strerror(errno));
	valid = 0;
	return;
    }

    if (nfilesys != sts) {
	/* initialization or something has changed */
	if (nfilesys == -1) {
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "Info: refresh_filesys_metrics: initial nfilesys=%d\n", sts);
	    nfilesys = sts;
	    pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_LOAD);
	}
	else {
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "Info: refresh_filesys_metrics: nfilesys changed from %d to %d\n", nfilesys, sts);
	    nfilesys = sts;
	    pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_INACTIVE);
	}
	if (stats != NULL)
	    free(stats);
	stats = (struct statfs *)malloc(nfilesys*sizeof(struct statfs));
	if (stats == NULL) {
	    __pmNoMem("refresh_filesys_metrics: stats", nfilesys*sizeof(struct statfs), PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	/* fetch all the available data */
	if ((sts = getfsstat(stats, nfilesys*sizeof(struct statfs), MNT_NOWAIT)) < 0) {
	    fprintf(stderr, "refresh_filesys_metrics: changed stats getfsstat(): %s\n", strerror(errno));
	    valid = 0;
	    return;
	}
	skipped_filesys = 0;
	for (i = 0; i < nfilesys; i++) {
	    /*
	     * only some "filesystem" types are of interest ... refer
	     * to fstab(5)
	     */
	    if (strcmp(stats[i].f_fstypename, "ffs") == 0 ||
	        strcmp(stats[i].f_fstypename, "ext2fs") == 0 ||
	        strcmp(stats[i].f_fstypename, "lfs") == 0) {
		    if ((sts = pmdaCacheStore(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_ADD, stats[i].f_mntfromname, &stats[i])) < 0) {
			fprintf(stderr, "refresh_filesys_metrics: pmdaCacheStore(%s) failed: %s\n", stats[i].f_mntfromname, pmErrStr(sts));
			skipped_filesys++;
			continue;
		    }
		}
	    else
		skipped_filesys++;
	}
	pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_SAVE);
    }
    else {
	/*
	 * no change in the number of statfs structs avaiable,
	 * assume the order remains the same, so just get 'em
	 */
	if ((sts = getfsstat(stats, nfilesys*sizeof(struct statfs), MNT_NOWAIT)) < 0) {
	    fprintf(stderr, "refresh_filesys_metrics: stats getfsstat(): %s\n", strerror(errno));
	    valid = 0;
	    return;
	}
    }

    valid = 1;
}

int
do_filesys_metrics(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    int			sts;
    struct statfs	*sp;

    if (!valid)
	return 0;

    if (inst != PM_IN_NULL) {
	/*
	 * per-filesys metrics
	 */
	sts = pmdaCacheLookup(indomtab[FILESYS_INDOM].it_indom, inst, NULL, (void **)&sp);
	if (sts == PMDA_CACHE_ACTIVE) {
	    sts = 1;
	    /* cluster and domain already checked, just need item ... */
	    switch (pmid_item(mdesc->m_desc.pmid)) {

		case 1:		/* filesys.capcity */
		    atom->ull = ((uint64_t)sp->f_blocks*sp->f_bsize)/1024;
		    break;

		case 2:		/* filesys.used */
		    atom->ull = ((uint64_t)(sp->f_blocks-sp->f_bfree)*sp->f_bsize)/1024;
		    break;

		case 3:		/* filesys.free */
		    atom->ull = ((uint64_t)sp->f_bfree*sp->f_bsize)/1024;
		    break;

		case 4:		/* filesys.maxfiles */
		    atom->ul = sp->f_files;
		    break;

		case 5:		/* filesys.usedfiles */
		    atom->ul = sp->f_files - sp->f_ffree;
		    break;

		case 6:		/* filesys.freefiles */
		    atom->ul = sp->f_ffree;
		    break;

		case 7:		/* filesys.mountdir */
		    atom->cp = sp->f_mntonname;
		    break;

		case 8:		/* filesys.full */
		    atom->d = 100.0 * (double)(sp->f_blocks-sp->f_bfree) / (double)sp->f_blocks;
		    break;

		case 9:		/* filesys.blocksize */
		    atom->ul = sp->f_bsize;
		    break;

		case 10:	/* filesys.avail */
		    atom->ull = ((uint64_t)sp->f_bavail*sp->f_bsize)/1024;
		    break;

		case 11:	/* filesys.readonly */
		    atom->ul = (sp->f_flags & MNT_RDONLY) == MNT_RDONLY;
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
	 * most filesys metrics don't have an instance domain
	 *
	 * cluster and domain already checked, just need item ...
	 */
	switch (pmid_item(mdesc->m_desc.pmid)) {
	    case 0:		/* hinv.nfilesys */
		atom->ul = nfilesys - skipped_filesys;
		sts = 1;
		break;

	    default:
		sts = PM_ERR_INST;
		break;
	}
    }

    return sts;
}
