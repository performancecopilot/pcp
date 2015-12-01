/*
 * NetBSD Kernel PMDA - filesys metrics
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
#include "netbsd.h"
#include <sys/statvfs.h>
#include <errno.h>
#include <string.h>

static int		nfilesys = -1;
static int		skipped_filesys = 0;
static struct statvfs	*stats;
static int		valid;

void
refresh_filesys_metrics(void)
{
    int		sts;
    int		i;

    /* get number of statvfs structs available */
    if ((sts = getvfsstat(NULL, 0, ST_NOWAIT)) < 0) {
	fprintf(stderr, "refresh_filesys_metrics: nfilesys getvfsstat(): %s\n", strerror(errno));
	valid = 0;
	return;
    }

    if (nfilesys != sts) {
	/* initialization or something has changed */
	if (nfilesys == -1) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "Info: refresh_filesys_metrics: initial nfilesys=%d\n", sts);
#endif
	    nfilesys = sts;
	    pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_LOAD);
	}
	else {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "Info: refresh_filesys_metrics: nfilesys changed from %d to %d\n", nfilesys, sts);
#endif
	    nfilesys = sts;
	    pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_INACTIVE);
	}
	if (stats != NULL)
	    free(stats);
	stats = (struct statvfs *)malloc(nfilesys*sizeof(struct statvfs));
	if (stats == NULL) {
	    __pmNoMem("refresh_filesys_metrics: stats", nfilesys*sizeof(struct statvfs), PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	/* fetch all the available data */
	if ((sts = getvfsstat(stats, nfilesys*sizeof(struct statvfs), ST_NOWAIT)) < 0) {
	    fprintf(stderr, "refresh_filesys_metrics: changed stats getvfsstat(): %s\n", strerror(errno));
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
	 * no change in the number of statvfs structs avaiable,
	 * assume the order remains the same, so just get 'em
	 */
	if ((sts = getvfsstat(stats, nfilesys*sizeof(struct statvfs), ST_NOWAIT)) < 0) {
	    fprintf(stderr, "refresh_filesys_metrics: stats getvfsstat(): %s\n", strerror(errno));
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
    struct statvfs	*sp;

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
		    atom->ull = ((uint64_t)sp->f_blocks*sp->f_frsize)/1024;
		    break;

		case 2:		/* filesys.used */
		    atom->ull = ((uint64_t)(sp->f_blocks-sp->f_bfree)*sp->f_frsize)/1024;
		    break;

		case 3:		/* filesys.free */
		    atom->ull = ((uint64_t)sp->f_bfree*sp->f_frsize)/1024;
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
		    atom->ull = ((uint64_t)sp->f_bavail*sp->f_frsize)/1024;
		    break;

		case 11:	/* filesys.readonly */
		    atom->ul = (sp->f_flag & ST_RDONLY) == ST_RDONLY;
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
