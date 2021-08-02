/*
 * Copyright (c) 2021 Red Hat.  All Rights Reserved.
 * Copyright (c) 2013 Ken McDonell, Inc.  All Rights Reserved.
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

#include <sys/stat.h>
#include "pmapi.h"
#include "libpcp.h"
#include "logcheck.h"

/*
 * check the temporal archname.index
 */

#ifdef __PCP_EXPERIMENTAL_ARCHIVE_VERSION3
static int
log3_pass1(__pmLogCtl *log, char *archname)
{
    int			i;
    char		path[MAXPATHLEN];
    off_t		meta_size = -1;
    off_t		log_size = -1;
    struct stat		sbuf;
    __pmLogTI3		*tip3;
    __pmLogTI3		*lastp;

    lastp = NULL;
    for (i = 1; i <= log->l_numti; i++) {

	/*
	 * Integrity Checks
	 *
	 * this(tv_sec) < 0
	 * this(tv_usec) < 0 || this(tv_nsec) > 999999999
	 * this(timestamp) < last(timestamp)
	 * this(timestamp) >= label timestamp
	 * this(vol) >= 0
	 * this(vol) < last(vol)
	 * this(vol) == last(vol) && this(meta) <= last(meta)
	 * this(vol) == last(vol) && this(log) <= last(log)
	 * file_exists(<base>.meta) && this(meta) > file_size(<base>.meta)
	 * file_exists(<base>.this(vol)) &&
	 *		this(log) > file_size(<base>.this(vol))
	 *
	 * Integrity Warnings
	 *
	 * this(vol) != last(vol) && !file_exists(<base>.this(vol))
	 */
	tip3 = &log->l_ti[i-1].v3;
	if (i == 1) {
	    __pmFILE *fp;
	    meta_size = -1;
	    pmsprintf(path, sizeof(path), "%s.meta", archname);
	    fp = __pmFopen(path, "r");
	    if (fp != NULL) {
	        if (__pmFstat(fp, &sbuf) == 0)
		    meta_size = sbuf.st_size;
		__pmFclose(fp);
	    }
	    if (meta_size == -1) {
		/*
		 * only get here if file exists, but __pmFopen() fails,
		 * e.g. for decompression failure
		 */
		fprintf(stderr, "%s: pass1: botch: cannot open metadata file (%s)\n", pmGetProgname(), path);
		exit(1);
	    }
	}
	if (tip3->ti_vol < 0) {
	    fprintf(stderr, "%s.index[entry %d]: illegal negative volume number %d\n",
		    archname, i, tip3->ti_vol);
	    index_state = STATE_BAD;
	    log_size = -1;
	}
	else if (lastp == NULL || tip3->ti_vol != lastp->ti_vol) { 
	    __pmFILE *fp;
	    log_size = -1;
	    pmsprintf(path, sizeof(path), "%s.%d", archname, tip3->ti_vol);
	    fp = __pmFopen(path, "r");
	    if (fp != NULL) {
	        if (__pmFstat(fp, &sbuf) == 0)
		    log_size = sbuf.st_size;
		__pmFclose(fp);
	    }
	    if (log_size == -1) {
		fprintf(stderr, "%s: file missing for log volume %d\n", path, tip3->ti_vol);
	    }
	}
	if (tip3->ti_sec < 0 || tip3->ti_nsec < 0) {
	    fprintf(stderr, "%s.index[entry %d]: illegal negative timestamp value (%lld sec, %ld nsec)\n",
		archname, i, (long long)tip3->ti_sec, (long)tip3->ti_nsec);
	    index_state = STATE_BAD;
	}
	if (tip3->ti_nsec > 999999999) {
	    fprintf(stderr, "%s.index[entry %d]: illegal timestamp nsec value (%lld sec, %ld nsec)\n",
		archname, i, (long long)tip3->ti_sec, (long)tip3->ti_nsec);
	    index_state = STATE_BAD;
	}
	/* TODO: adjust for dynamic v3 archive label size */
	if (tip3->ti_meta < sizeof(__pmLogLabel)+2*sizeof(int)) {
	    fprintf(stderr, "%s.index[entry %d]: offset to metadata (%lld) before end of label record (%zu)\n",
		archname, i, (long long)tip3->ti_meta, sizeof(__pmLogLabel)+2*sizeof(int));
	    index_state = STATE_BAD;
	}
	if (meta_size != -1 && tip3->ti_meta > meta_size) {
	    fprintf(stderr, "%s.index[entry %d]: offset to metadata (%lld) past end of file (%lld)\n",
		archname, i, (long long)tip3->ti_meta, (long long)meta_size);
	    index_state = STATE_BAD;
	}
	/* TODO: adjust for dynamic v3 archive label size */
	if (tip3->ti_log < sizeof(__pmLogLabel)+2*sizeof(int)) {
	    fprintf(stderr, "%s.index[entry %d]: offset to log (%lld) before end of label record (%zu)\n",
		archname, i, (long long)tip3->ti_log, sizeof(__pmLogLabel)+2*sizeof(int));
	    index_state = STATE_BAD;
	}
	if (log_size != -1 && tip3->ti_log > log_size) {
	    fprintf(stderr, "%s.index[entry %d]: offset to log (%lld) past end of file (%lld)\n",
		archname, i, (long long)tip3->ti_log, (long long)log_size);
	    index_state = STATE_BAD;
	}
	/* TODO: adjust for v3 archive label timespec */
#if 0
	if (log_label.ill_start.tv_sec != 0) {
	    if (__pmTimevalSub(&tip->ti_stamp, &log_label.ill_start) < 0) {
		fprintf(stderr, "%s.index[entry %d]: timestamp (%lld.%09d) less than log label timestamp (%lld.%09d)\n",
			archname, i,
			(long long)tip3->ti_sec, (long)tip->ti_nsec,
			(long long)log_label.ill_start.tv_sec, (long)log_label.ill_start.tv_usec);
		index_state = STATE_BAD;
	    }
	}
#endif
	if (lastp != NULL) {
	    pmTimespec	this_stamp, last_stamp;

	    this_stamp.tv_sec = tip3->ti_sec;
	    this_stamp.tv_nsec = tip3->ti_nsec;
	    last_stamp.tv_sec = lastp->ti_sec;
	    last_stamp.tv_nsec = lastp->ti_nsec;
	    if (__pmTimespecSub(&this_stamp, &last_stamp) < 0) {
		fprintf(stderr, "%s.index[entry %d]: timestamp (%lld.%09d) went backwards in time (from %lld.%09d at [entry %d])\n",
			archname, i,
			(long long)tip3->ti_sec, tip3->ti_nsec,
			(long long)lastp->ti_sec, lastp->ti_nsec, i-1);
		index_state = STATE_BAD;
	    }
	    if (tip3->ti_vol < lastp->ti_vol) {
		fprintf(stderr, "%s.index[entry %d]: volume number (%d) decreased (from %d at [entry %d])\n",
			archname, i, tip3->ti_vol, lastp->ti_vol, i-1);
		index_state = STATE_BAD;
	    }
	    if (tip3->ti_vol == lastp->ti_vol && tip3->ti_meta < lastp->ti_meta) {
		fprintf(stderr, "%s.index[entry %d]: offset to metadata (%lld) decreased (from %lld at [entry %d])\n",
			archname, i, (long long)tip3->ti_meta, (long long)lastp->ti_meta, i-1);
		index_state = STATE_BAD;
	    }
	    if (tip3->ti_vol == lastp->ti_vol && tip3->ti_log < lastp->ti_log) {
		fprintf(stderr, "%s.index[entry %d]: offset to log (%lld) decreased (from %lld at [entry %d])\n",
			archname, i, (long long)tip3->ti_log, (long long)lastp->ti_log, i-1);
		index_state = STATE_BAD;
	    }
	}
	lastp = tip3;
    }

    return STS_OK;
}
#endif

static int
log2_pass1(__pmLogCtl *log, char *archname)
{
    int			i;
    char		path[MAXPATHLEN];
    off_t		meta_size = -1;
    off_t		log_size = -1;
    struct stat		sbuf;
    __pmLogTI2		*tip2;
    __pmLogTI2		*lastp;

    lastp = NULL;
    for (i = 1; i <= log->l_numti; i++) {

	/*
	 * Integrity Checks
	 *
	 * this(tv_sec) < 0
	 * this(tv_usec) < 0 || this(tv_usec) > 999999
	 * this(timestamp) < last(timestamp)
	 * this(timestamp) >= label timestamp
	 * this(vol) >= 0
	 * this(vol) < last(vol)
	 * this(vol) == last(vol) && this(meta) <= last(meta)
	 * this(vol) == last(vol) && this(log) <= last(log)
	 * file_exists(<base>.meta) && this(meta) > file_size(<base>.meta)
	 * file_exists(<base>.this(vol)) &&
	 *		this(log) > file_size(<base>.this(vol))
	 *
	 * Integrity Warnings
	 *
	 * this(vol) != last(vol) && !file_exists(<base>.this(vol))
	 */
	tip2 = &log->l_ti[i-1].v2;
	if (i == 1) {
	    __pmFILE *fp;
	    meta_size = -1;
	    pmsprintf(path, sizeof(path), "%s.meta", archname);
	    fp = __pmFopen(path, "r");
	    if (fp != NULL) {
	        if (__pmFstat(fp, &sbuf) == 0)
		    meta_size = sbuf.st_size;
		__pmFclose(fp);
	    }
	    if (meta_size == -1) {
		/*
		 * only get here if file exists, but __pmFopen() fails,
		 * e.g. for decompression failure
		 */
		fprintf(stderr, "%s: pass1: botch: cannot open metadata file (%s)\n", pmGetProgname(), path);
		exit(1);
	    }
	}
	if (tip2->ti_vol < 0) {
	    fprintf(stderr, "%s.index[entry %d]: illegal negative volume number %d\n",
		    archname, i, tip2->ti_vol);
	    index_state = STATE_BAD;
	    log_size = -1;
	}
	else if (lastp == NULL || tip2->ti_vol != lastp->ti_vol) { 
	    __pmFILE *fp;
	    log_size = -1;
	    pmsprintf(path, sizeof(path), "%s.%d", archname, tip2->ti_vol);
	    fp = __pmFopen(path, "r");
	    if (fp != NULL) {
	        if (__pmFstat(fp, &sbuf) == 0)
		    log_size = sbuf.st_size;
		__pmFclose(fp);
	    }
	    if (log_size == -1) {
		fprintf(stderr, "%s: file missing for log volume %d\n", path, tip2->ti_vol);
	    }
	}
	if (tip2->ti_stamp.tv_sec < 0 || tip2->ti_stamp.tv_usec < 0) {
	    fprintf(stderr, "%s.index[entry %d]: illegal negative timestamp value (%d sec, %d usec)\n",
		archname, i, tip2->ti_stamp.tv_sec, tip2->ti_stamp.tv_usec);
	    index_state = STATE_BAD;
	}
	if (tip2->ti_stamp.tv_usec > 999999) {
	    fprintf(stderr, "%s.index[entry %d]: illegal timestamp usec value (%d sec, %d usec)\n",
		archname, i, tip2->ti_stamp.tv_sec, tip2->ti_stamp.tv_usec);
	    index_state = STATE_BAD;
	}
	if (tip2->ti_meta < sizeof(__pmLogLabel)+2*sizeof(int)) {
	    fprintf(stderr, "%s.index[entry %d]: offset to metadata (%ld) before end of label record (%ld)\n",
		archname, i, (long)tip2->ti_meta, (long)(sizeof(__pmLogLabel)+2*sizeof(int)));
	    index_state = STATE_BAD;
	}
	if (meta_size != -1 && tip2->ti_meta > meta_size) {
	    fprintf(stderr, "%s.index[entry %d]: offset to metadata (%ld) past end of file (%ld)\n",
		archname, i, (long)tip2->ti_meta, (long)meta_size);
	    index_state = STATE_BAD;
	}
	if (tip2->ti_log < sizeof(__pmLogLabel)+2*sizeof(int)) {
	    fprintf(stderr, "%s.index[entry %d]: offset to log (%ld) before end of label record (%ld)\n",
		archname, i, (long)tip2->ti_log, (long)(sizeof(__pmLogLabel)+2*sizeof(int)));
	    index_state = STATE_BAD;
	}
	if (log_size != -1 && tip2->ti_log > log_size) {
	    fprintf(stderr, "%s.index[entry %d]: offset to log (%ld) past end of file (%ld)\n",
		archname, i, (long)tip2->ti_log, (long)log_size);
	    index_state = STATE_BAD;
	}
	if (log_label.ill_start.tv_sec != 0) {
	    if (__pmTimevalSub(&tip2->ti_stamp, &log_label.ill_start) < 0) {
		fprintf(stderr, "%s.index[entry %d]: timestamp (%d.%06d) less than log label timestamp (%d.%06d)\n",
			archname, i,
			(int)tip2->ti_stamp.tv_sec, (int)tip2->ti_stamp.tv_usec,
			(int)log_label.ill_start.tv_sec, (int)log_label.ill_start.tv_usec);
		index_state = STATE_BAD;
	    }
	}
	if (lastp != NULL) {
	    if (__pmTimevalSub(&tip2->ti_stamp, &lastp->ti_stamp) < 0) {
		fprintf(stderr, "%s.index[entry %d]: timestamp (%d.%06d) went backwards in time (from %d.%06d at [entry %d])\n",
			archname, i,
			(int)tip2->ti_stamp.tv_sec, (int)tip2->ti_stamp.tv_usec,
			(int)lastp->ti_stamp.tv_sec, (int)lastp->ti_stamp.tv_usec, i-1);
		index_state = STATE_BAD;
	    }
	    if (tip2->ti_vol < lastp->ti_vol) {
		fprintf(stderr, "%s.index[entry %d]: volume number (%d) decreased (from %d at [entry %d])\n",
			archname, i, tip2->ti_vol, lastp->ti_vol, i-1);
		index_state = STATE_BAD;
	    }
	    if (tip2->ti_vol == lastp->ti_vol && tip2->ti_meta < lastp->ti_meta) {
		fprintf(stderr, "%s.index[entry %d]: offset to metadata (%ld) decreased (from %ld at [entry %d])\n",
			archname, i, (long)tip2->ti_meta, (long)lastp->ti_meta, i-1);
		index_state = STATE_BAD;
	    }
	    if (tip2->ti_vol == lastp->ti_vol && tip2->ti_log < lastp->ti_log) {
		fprintf(stderr, "%s.index[entry %d]: offset to log (%ld) decreased (from %ld at [entry %d])\n",
			archname, i, (long)tip2->ti_log, (long)lastp->ti_log, i-1);
		index_state = STATE_BAD;
	    }
	}
	lastp = tip2;
    }

    return STS_OK;
}

int
pass1(__pmContext *ctxp, char *archname)
{
    __pmLogCtl	*log = ctxp->c_archctl->ac_log;

    if (vflag)
	fprintf(stderr, "%s: start pass1 (check temporal index)\n", archname);

    if (log->l_numti <= 0) {
	fprintf(stderr, "%s: warning temporal index is missing\n", archname);
	return STS_WARNING;
    }

#ifdef __PCP_EXPERIMENTAL_ARCHIVE_VERSION3
    if (__pmLogVersion(log) >= PM_LOG_VERS03)
	return log3_pass1(log, archname);
#endif
    return log2_pass1(log, archname);
}
