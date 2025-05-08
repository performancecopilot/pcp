/*
 * Copyright (c) 2012-2017,2020-2021 Red Hat.
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2021, Ken McDonell.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * Thread-safe notes:
 *
 * __pmLogReads is a diagnostic counter that is maintained with
 * non-atomic updates ... we've decided that it is acceptable for the
 * value to be subject to possible (but unlikely) missed updates
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#include <sys/stat.h>
#include <assert.h>

/*
 * On-Disk Temporal Index Record, Version 3
 */
typedef struct {
    __int32_t	sec[2];		/* __pmTimestamp */
    __int32_t	nsec;
    __int32_t	vol;
    __int32_t	off_meta[2];
    __int32_t	off_data[2];
} __pmTI_v3;

/*
 * On-Disk Temporal Index Record, Version 2
 */
typedef struct {
    __int32_t	sec;		/* pmTimeval */
    __int32_t	usec;
    __int32_t	vol;
    __int32_t	off_meta;
    __int32_t	off_data;
} __pmTI_v2;

/*
 * This condition (off_data == 0) been seen in QA where pmlogger churns
 * quickly ... trying to understand why using this diagnostic.
 */
static void
__pmLogIndexZeroTILogDiagnostic(const __pmArchCtl *acp, const char *caller)
{
    struct stat	sbuf;
    int		sts;

    fprintf(stderr, "%s: Botch: log offset == 0\n", caller);
    fprintf(stderr, "  __pmFileno=%d __pmFtell -> %lld\n",
		    __pmFileno(acp->ac_mfp),
		    (long long)acp->ac_tell_cb(acp, 0, caller));
    if ((sts = __pmFstat(acp->ac_mfp, &sbuf)) < 0)
	fprintf(stderr, "  __pmFstat failed -> %d\n", sts);
    else
	fprintf(stderr, "  __pmFstat st_size=%lld st_ino=%lld\n",
			(long long)sbuf.st_size, (long long)sbuf.st_ino);
}

/* Emit a Log Version 3 Temporal Index entry */
static int
__pmLogPutIndex_v3(const __pmArchCtl *acp, const __pmTimestamp * const tsp)
{
    const char		caller[] = "__pmLogPutIndex";
    __pmTI_v3		ti;
    __pmoff64_t		off_meta;
    __pmoff64_t		off_data;
    int			sts;

    ti.vol = acp->ac_curvol;
    off_meta = (__pmoff64_t)acp->ac_tell_cb(acp, PM_LOG_VOL_META, caller);
    memcpy((void *)&ti.off_meta[0], (void *)&off_meta, 2*sizeof(__int32_t));
    off_data = (__pmoff64_t)acp->ac_tell_cb(acp, 0, caller);
    if (off_data == 0)
	__pmLogIndexZeroTILogDiagnostic(acp, caller);
    memcpy((void *)&ti.off_data[0], (void *)&off_data, 2*sizeof(__int32_t));

    if (pmDebugOptions.log)
	fprintf(stderr, "%s: timestamp=%" FMT_INT64 ".09%d vol=%d"
		" meta posn=%" FMT_INT64 " log posn=%" FMT_INT64 "\n",
		caller, tsp->sec, tsp->nsec, ti.vol, off_meta, off_data);

    __pmPutTimestamp(tsp, &ti.sec[0]);
    ti.vol = htonl(ti.vol);
    __htonll((char *)&ti.off_meta[0]);
    __htonll((char *)&ti.off_data[0]);

    if ((sts = acp->ac_write_cb(acp, PM_LOG_VOL_TI, &ti, sizeof(ti), caller)) < 0)
	return sts;
    if ((sts = acp->ac_flush_cb(acp, PM_LOG_VOL_TI, caller)) < 0)
	return sts;
    return 0;
}

/* Emit a Log Version 2 Temporal Index entry */
static int
__pmLogPutIndex_v2(const __pmArchCtl *acp, const __pmTimestamp *tsp)
{
    const char		caller[] = "__pmLogPutIndex";
    __pmTI_v2		ti;
    __pmoff64_t		off_meta;
    __pmoff64_t		off_data;
    int			sts;

    ti.vol = acp->ac_curvol;

    if (sizeof(off_t) > sizeof(__pmoff32_t)) {
	/* check for overflow of the offset ... */
	off_t	tmp;

	tmp = acp->ac_tell_cb(acp, PM_LOG_VOL_META, caller);
	assert(tmp >= 0);
	off_meta = (__pmoff32_t)tmp;
	if (tmp != off_meta) {
	    pmNotifyErr(LOG_ERR,
			"%s: PCP archive file (%s) too big\n", caller, "meta");
	    return -E2BIG;
	}
	tmp = acp->ac_tell_cb(acp, 0, caller);
	assert(tmp >= 0);
	off_data = (__pmoff32_t)tmp;
	if (tmp != off_data) {
	    pmNotifyErr(LOG_ERR,
			"%s: PCP archive file (%s) too big\n", caller, "data");
	    return -E2BIG;
	}
    }
    else {
	off_meta = (__pmoff32_t)acp->ac_tell_cb(acp, PM_LOG_VOL_META, caller);
	off_data = (__pmoff32_t)acp->ac_tell_cb(acp, 0, caller);
    }

    if (off_data == 0)
	__pmLogIndexZeroTILogDiagnostic(acp, caller);

    if (pmDebugOptions.log) {
	fprintf(stderr, "%s: timestamp=%" FMT_INT64 ".06%d vol=%d meta posn=%" FMT_INT64 " log posn=%" FMT_INT64 "\n",
	    caller, tsp->sec, tsp->nsec / 1000, ti.vol, off_meta, off_data);
    }

    __pmPutTimeval(tsp, &ti.sec);
    ti.vol = htonl(ti.vol);
    ti.off_meta = htonl((__int32_t)off_meta);
    ti.off_data = htonl((__int32_t)off_data);

    if ((sts = acp->ac_write_cb(acp, PM_LOG_VOL_TI, &ti, sizeof(ti), caller)) < 0)
	return sts;
    if ((sts = acp->ac_flush_cb(acp, PM_LOG_VOL_TI, caller)) < 0)
	return sts;
    return 0;
}

int
__pmLogPutIndex(const __pmArchCtl *acp, const __pmTimestamp *tsp)
{
    struct timespec	tmp;
    __pmTimestamp	stamp;
    __pmLogCtl		*lcp = acp->ac_log;

    acp->ac_flush_cb(acp, PM_LOG_VOL_META, __FUNCTION__);
    acp->ac_flush_cb(acp, 0, __FUNCTION__);

    if (tsp == NULL) {
	pmtimespecNow(&tmp);
	stamp.sec = tmp.tv_sec;
	stamp.nsec = tmp.tv_nsec;
	tsp = &stamp;
    }

    if (__pmLogVersion(lcp) == PM_LOG_VERS03)
	return __pmLogPutIndex_v3(acp, tsp);
    else if (__pmLogVersion(lcp) == PM_LOG_VERS02)
	return __pmLogPutIndex_v2(acp, tsp);
    else
	return PM_ERR_LABEL;
}

int
__pmLogLoadIndex(__pmLogCtl *lcp)
{
    int		sts = 0;
    __pmFILE	*f = lcp->tifp;
    size_t	record_size;
    size_t	bytes;
    void	*buffer;
    __pmLogTI	*tip;

    lcp->numti = 0;
    lcp->ti = NULL;

    if (__pmLogVersion(lcp) == PM_LOG_VERS03)
	record_size = sizeof(__pmTI_v3);
    else if (__pmLogVersion(lcp) == PM_LOG_VERS02)
	record_size = sizeof(__pmTI_v2);
    else
	return PM_ERR_LABEL;

    if ((buffer = (void *)malloc(record_size)) == NULL) {
	pmNoMem("__pmLogLoadIndex: buffer", record_size, PM_RECOV_ERR);
	return -oserror();
    }

    if (lcp->tifp != NULL) {
	__pmFseek(f, (long)__pmLogLabelSize(lcp), SEEK_SET);
	for ( ; ; ) {
	    __pmLogTI	*tmp;
	    bytes = (1 + lcp->numti) * sizeof(__pmLogTI);
	    tmp = (__pmLogTI *)realloc(lcp->ti, bytes);
	    if (tmp == NULL) {
		pmNoMem("__pmLogLoadIndex: realloc TI", bytes, PM_FATAL_ERR);
		sts = -oserror();
		goto bad;
	    }
	    lcp->ti = tmp;
	    bytes = __pmFread(buffer, 1, record_size, f);
	    if (bytes != record_size) {
		if (__pmFeof(f)) {
		    __pmClearerr(f);
		    sts = 0; 
		    break;
		}
	  	if (pmDebugOptions.log)
	    	    fprintf(stderr, "%s: bad TI entry len=%zu: expected %zu\n",
			    "__pmLogLoadIndex", bytes, record_size);
		if (__pmFerror(f)) {
		    __pmClearerr(f);
		    sts = -oserror();
		    goto bad;
		}
		else {
		    sts = PM_ERR_LOGREC;
		    goto bad;
		}
	    }
	    tip = &lcp->ti[lcp->numti];
	    /*
	     * swab and copy fields
	     */
	    if (__pmLogVersion(lcp) == PM_LOG_VERS03) {
		__pmTI_v3	*tip_v3 = (__pmTI_v3 *)buffer;
		__pmLoadTimestamp(&tip_v3->sec[0], &tip->stamp);
		tip->vol = ntohl(tip_v3->vol);
		__ntohll((char *)&tip_v3->off_meta[0]);
		__ntohll((char *)&tip_v3->off_data[0]);
		memcpy((void *)&tip->off_meta, (void *)&tip_v3->off_meta[0], 2*sizeof(__int32_t));
		memcpy((void *)&tip->off_data, (void *)&tip_v3->off_data[0], 2*sizeof(__int32_t));
	    }
	    else {
		/* __pmLogVersion(lcp) == PM_LOG_VERS02 */
		__pmTI_v2	*tip_v2 = (__pmTI_v2 *)buffer;
		__pmLoadTimeval(&tip_v2->sec, &tip->stamp);
		tip->vol = ntohl(tip_v2->vol);
		tip->off_meta = ntohl(tip_v2->off_meta);
		tip->off_data = ntohl(tip_v2->off_data);
	    }

	    lcp->numti++;
	}
    }
    free(buffer);
    return sts;

bad:
    if (lcp->ti != NULL) {
	free(lcp->ti);
	lcp->ti = NULL;
    }
    lcp->numti = 0;
    free(buffer);
    return sts;
}
