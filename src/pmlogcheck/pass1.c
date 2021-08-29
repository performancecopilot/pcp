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
int
pass1(__pmContext *ctxp, char *archname)
{
    int			i;
    char		path[MAXPATHLEN];
    off_t		meta_size = -1;
    off_t		log_size = -1;
    struct stat		sbuf;
    __pmLogTI		*tip;
    __pmLogTI		*lastp;
    __pmLogCtl		*log = ctxp->c_archctl->ac_log;

    if (vflag)
	fprintf(stderr, "%s: start pass1 (check temporal index)\n", archname);

    if (log->numti <= 0) {
	fprintf(stderr, "%s: warning temporal index is missing\n", archname);
	return STS_WARNING;
    }


    lastp = NULL;
    for (i = 1; i <= log->numti; i++) {

	/*
	 * Integrity Checks
	 *
	 * this(sec) < 0
	 * this(nsec) < 0 || this(nsec) > 999999999
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
	tip = &log->ti[i-1];
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
	if (tip->vol < 0) {
	    fprintf(stderr, "%s.index[entry %d]: illegal negative volume number %d\n",
		    archname, i, tip->vol);
	    index_state = STATE_BAD;
	    log_size = -1;
	}
	else if (lastp == NULL || tip->vol != lastp->vol) { 
	    __pmFILE *fp;
	    log_size = -1;
	    pmsprintf(path, sizeof(path), "%s.%d", archname, tip->vol);
	    fp = __pmFopen(path, "r");
	    if (fp != NULL) {
	        if (__pmFstat(fp, &sbuf) == 0)
		    log_size = sbuf.st_size;
		__pmFclose(fp);
	    }
	    if (log_size == -1) {
		fprintf(stderr, "%s: file missing for log volume %d\n", path, tip->vol);
	    }
	}
	if (tip->stamp.sec < 0 || tip->stamp.nsec < 0) {
	    fprintf(stderr, "%s.index[entry %d]: illegal negative timestamp value (%" FMT_INT64 " sec, %d nsec)\n",
		archname, i, tip->stamp.sec, tip->stamp.nsec);
	    index_state = STATE_BAD;
	}
	if (tip->stamp.nsec > 999999999) {
	    fprintf(stderr, "%s.index[entry %d]: illegal timestamp nsec value (%" FMT_INT64 " sec, %d nsec)\n",
		archname, i, tip->stamp.sec, tip->stamp.nsec);
	    index_state = STATE_BAD;
	}
	if (tip->off_meta < __pmLogLabelSize(log)) {
	    fprintf(stderr, "%s.index[entry %d]: offset to metadata (%lld) before end of label record (%zd)\n",
		archname, i, (long long)tip->off_meta, __pmLogLabelSize(log));
	    index_state = STATE_BAD;
	}
	if (meta_size != -1 && tip->off_meta > meta_size) {
	    fprintf(stderr, "%s.index[entry %d]: offset to metadata (%lld) past end of file (%lld)\n",
		archname, i, (long long)tip->off_meta, (long long)meta_size);
	    index_state = STATE_BAD;
	}
	if (tip->off_data < __pmLogLabelSize(log)) {
	    fprintf(stderr, "%s.index[entry %d]: offset to log (%lld) before end of label record (%zd)\n",
		archname, i, (long long)tip->off_data, __pmLogLabelSize(log));
	    index_state = STATE_BAD;
	}
	if (log_size != -1 && tip->off_data > log_size) {
	    fprintf(stderr, "%s.index[entry %d]: offset to log (%lld) past end of file (%lld)\n",
		archname, i, (long long)tip->off_data, (long long)log_size);
	    index_state = STATE_BAD;
	}
	if (goldenstart.sec != 0) {
	    if (__pmTimestampSub(&tip->stamp, &goldenstart) < 0) {
		fprintf(stderr, "%s.index[entry %d]: timestamp (%" FMT_INT64 ".%09d) less than log label timestamp (%" FMT_INT64 ".%09d)\n",
			archname, i,
			tip->stamp.sec, tip->stamp.nsec,
			goldenstart.sec, goldenstart.nsec);
		index_state = STATE_BAD;
	    }
	}
	if (lastp != NULL) {
	    if (__pmTimestampSub(&tip->stamp, &lastp->stamp) < 0) {
		fprintf(stderr, "%s.index[entry %d]: timestamp (%" FMT_INT64 ".%09d) went backwards in time (from %" FMT_INT64 ".%09d at [entry %d])\n",
			archname, i,
			tip->stamp.sec, tip->stamp.nsec,
			lastp->stamp.sec, lastp->stamp.nsec, i-1);
		index_state = STATE_BAD;
	    }
	    if (tip->vol < lastp->vol) {
		fprintf(stderr, "%s.index[entry %d]: volume number (%d) decreased (from %d at [entry %d])\n",
			archname, i, tip->vol, lastp->vol, i-1);
		index_state = STATE_BAD;
	    }
	    if (tip->vol == lastp->vol && tip->off_meta < lastp->off_meta) {
		fprintf(stderr, "%s.index[entry %d]: offset to metadata (%lld) decreased (from %lld at [entry %d])\n",
			archname, i, (long long)tip->off_meta, (long long)lastp->off_meta, i-1);
		index_state = STATE_BAD;
	    }
	    if (tip->vol == lastp->vol && tip->off_data < lastp->off_data) {
		fprintf(stderr, "%s.index[entry %d]: offset to log (%lld) decreased (from %lld at [entry %d])\n",
			archname, i, (long long)tip->off_data, (long long)lastp->off_data, i-1);
		index_state = STATE_BAD;
	    }
	}
	lastp = tip;
    }

    return STS_OK;
}
