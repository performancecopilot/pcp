/*
 * Copyright (c) 1995-2006,2008 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2021-2022 Red Hat.
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
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#include "fault.h"

static int
__pmUpdateProfile(int fd, __pmContext *ctxp, int timeout)
{
    int		sts;

    if (ctxp->c_sent == 0) {

	/*
	 * current profile is _not_ already cached at other end of
	 * IPC, so send the current profile
	 */
	if (pmDebugOptions.profile) {
	    fprintf(stderr, "pmFetch: calling __pmSendProfile, context: %d slot: %d\n",
	            ctxp->c_handle, ctxp->c_slot);
	    __pmDumpProfile(stderr, PM_INDOM_NULL, ctxp->c_instprof);
	}
	if ((sts = __pmSendProfile(fd, __pmPtrToHandle(ctxp),
				   ctxp->c_slot, ctxp->c_instprof)) < 0)
	    return sts;
	else
	    ctxp->c_sent = 1;
    }
    return 0;
}

static int
__pmRecvFetchPDU(int fd, __pmContext *ctxp, int timeout, int pdutype,
		__pmResult **result)
{
    __pmPDU	*pb;
    int		sts, pinpdu, changed = 0;

    do {
	sts = pinpdu = __pmGetPDU(fd, ANY_SIZE, timeout, &pb);
	if (sts == PDU_HIGHRES_RESULT && pdutype == PDU_HIGHRES_FETCH)
	    sts = __pmDecodeHighResResult_ctx(ctxp, pb, result);
	else if (sts == PDU_RESULT && pdutype == PDU_FETCH)
	    sts = __pmDecodeResult_ctx(ctxp, pb, result);
	else if (sts == PDU_ERROR) {
	    __pmDecodeError(pb, &sts);
	    if (sts > 0)
		/* PMCD state change protocol */
		changed |= sts;
	}
	else if (sts != PM_ERR_TIMEOUT) {
	    if (pmDebugOptions.pdu) {
		char	strbuf[20];
		char	errmsg[PM_MAXERRMSGLEN];
		if (sts < 0)
		    fprintf(stderr, "__pmRecvFetchPDU: PM_ERR_IPC: expecting PDU_RESULT or PDU_HIGHRES_FETCH but__pmGetPDU returns %d (%s)\n",
			sts, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		else
		    fprintf(stderr, "__pmRecvFetchPDU: PM_ERR_IPC: expecting PDU_RESULT or PDU_HIGHRES_FETCH but__pmGetPDU returns %d (type=%s)\n",
			sts, __pmPDUTypeStr_r(sts, strbuf, sizeof(strbuf)));
	    }
	    sts = PM_ERR_IPC;
	}

	if (pinpdu > 0)
	    __pmUnpinPDUBuf(pb);
    } while (sts > 0);

    if (sts == 0)
	return changed;
    return sts;
}

int
__pmPrepareFetch(__pmContext *ctxp, int numpmid, const pmID *ids, pmID **newids)
{
    return __dmprefetch(ctxp, numpmid, ids, newids);
}

int
__pmFinishResult(__pmContext *ctxp, int count, __pmResult **resultp)
{
    if (count >= 0)
	__dmpostfetch(ctxp, resultp);
    return count;
}

void
__pmDumpFetchFlags(FILE *f, int sts)
{
    int flag = 0;

    fprintf(f, "PMCD state: ");
    if (sts & PMCD_AGENT_CHANGE) {
	fprintf(f, "agent(s)");
	if (sts & PMCD_ADD_AGENT) fprintf(f, " added");
	if (sts & PMCD_RESTART_AGENT) fprintf(f, " restarted");
	if (sts & PMCD_DROP_AGENT) fprintf(f, " dropped");
	flag++;
    }
    if (sts & PMCD_LABEL_CHANGE) {
	if (flag++)
	    fprintf(f, ", ");
	fprintf(f, "label change");
    }
    if (sts & PMCD_NAMES_CHANGE) {
	if (flag++)
	    fprintf(f, ", ");
	fprintf(f, "names change");
    }
    if (sts & PMCD_HOSTNAME_CHANGE) {
	if (flag++)
	    fprintf(f, ", ");
	fprintf(f, "hostname change");
    }
}

static void
trace_fetch_entry(int numpmid, pmID *pmidlist)
{
    char    dbgbuf[20];

    fprintf(stderr, "%s(%d, pmid[0] %s", "pmFetch", numpmid,
		    pmIDStr_r(pmidlist[0], dbgbuf, sizeof(dbgbuf)));
    if (numpmid > 1)
	fprintf(stderr, " ... pmid[%d] %s", numpmid-1,
			pmIDStr_r(pmidlist[numpmid-1], dbgbuf, sizeof(dbgbuf)));
    fprintf(stderr, ", ...) <:");
}

static void
trace_fetch_exit(int sts)
{
    fprintf(stderr, ":> returns ");
    if (sts >= 0) {
	fprintf(stderr, "%d\n", sts);
    } else {
	char	errmsg[PM_MAXERRMSGLEN];
	fprintf(stderr, "%s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
    }
}

/*
 * Internal variant of pmFetch() API family ... ctxp is not NULL for
 * internal callers where the current context is already locked, but
 * NULL for callers from above the PMAPI or internal callers when the
 * current context is not locked.
 */
int
__pmFetch(__pmContext *ctxp, int numpmid, pmID *pmidlist, __pmResult **result)
{
    int		need_unlock = 0;
    int		ctx, sts;

    if (pmDebugOptions.pmapi)
	trace_fetch_entry(numpmid, pmidlist);

    if (numpmid < 1) {
	sts = PM_ERR_TOOSMALL;
	goto pmapi_return;
    }

    if ((sts = ctx = pmWhichContext()) >= 0) {
	pmID		*newlist = NULL;
	int		newcnt;
	int		fd, tout, have_dm, pdutype;

	if (ctxp == NULL) {
	    ctxp = __pmHandleToPtr(ctx);
	    if (ctxp != NULL)
		need_unlock = 1;
	}
	else
	    PM_ASSERT_IS_LOCKED(ctxp->c_lock);

	if (ctxp == NULL) {
	    sts = PM_ERR_NOCONTEXT;
	    goto pmapi_return;
	}

	/* local context requires single-threaded applications */
	if (ctxp->c_type == PM_CONTEXT_LOCAL &&
	    PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA)) {
	    sts = PM_ERR_THREAD;
	    goto pmapi_return;
	}

	/* for derived metrics, may need to rewrite the pmidlist */
	have_dm = newcnt = __pmPrepareFetch(ctxp, numpmid, pmidlist, &newlist);
	if (newcnt > numpmid) {
	    /* replace args passed into pmFetch */
	    numpmid = newcnt;
	    pmidlist = newlist;
	}

	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    /* find type of PDU we will send in live mode */
	    fd = ctxp->c_pmcd->pc_fd;
	    /* use high resolution timestamps whenever pmcd supports them */
	    if ((__pmFeaturesIPC(fd) & PDU_FLAG_HIGHRES))
		pdutype = PDU_HIGHRES_FETCH;
	    else
		pdutype = PDU_FETCH;
	    tout = ctxp->c_pmcd->pc_tout_sec;
	    if ((sts = __pmUpdateProfile(fd, ctxp, tout)) < 0)
		sts = __pmMapErrno(sts);
	    else if ((sts = __pmSendFetchPDU(fd, __pmPtrToHandle(ctxp),
				ctxp->c_slot, numpmid, pmidlist, pdutype)) < 0)
		sts = __pmMapErrno(sts);
	    else {
		PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_CALL);
		sts = __pmRecvFetchPDU(fd, ctxp, tout, pdutype, result);
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    sts = __pmFetchLocal(ctxp, numpmid, pmidlist, result);
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    sts = __pmLogFetch(ctxp, numpmid, pmidlist, result);
	    if (sts >= 0 && ctxp->c_mode != PM_MODE_INTERP)
		ctxp->c_origin = (*result)->timestamp;
	}

	/* process derived metrics, if any */
	if (have_dm)
	    __pmFinishResult(ctxp, sts, result);

	if (newlist != NULL)
	    free(newlist);
    }

pmapi_return:

    if (pmDebugOptions.pmapi)
	trace_fetch_exit(sts);

    if (pmDebugOptions.fetch) {
	fprintf(stderr, "%s returns ...\n", "pmFetch");
	if (sts >= 0) {
	    if (sts > 0) {
		__pmDumpFetchFlags(stderr, sts);
		fputc('\n', stderr);
	    }
	    __pmPrintResult_ctx(ctxp, stderr, *result);
	} else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "Error: %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }

    if (need_unlock)
	PM_UNLOCK(ctxp->c_lock);
    return sts;
}

int
pmFetch_ctx(__pmContext *ctxp, int numpmid, pmID *pmidlist, __pmResult **result)
{
    return __pmFetch(ctxp, numpmid, pmidlist, result);
}

int
pmFetch_v2(int numpmid, pmID *pmidlist, pmResult_v2 **result)
{
    __pmResult	*rp;
    int		sts;

    sts = pmFetch_ctx(NULL, numpmid, pmidlist, &rp);
    if (sts >= 0) {
	pmResult_v2	*ans = __pmOffsetResult_v2(rp);
	__pmTimestamp	tmp = rp->timestamp;	/* struct copy */

	ans->timestamp.tv_sec = tmp.sec;
	ans->timestamp.tv_usec = tmp.nsec / 1000;
	*result = ans;
    }
    return sts;
}

int
pmFetch(int numpmid, pmID *pmidlist, pmResult **result)
{
    __pmResult	*rp;
    int		sts;

    sts = pmFetch_ctx(NULL, numpmid, pmidlist, &rp);
    if (sts >= 0) {
	pmResult	*ans = __pmOffsetResult(rp);
	__pmTimestamp	tmp = rp->timestamp;	/* struct copy */

	ans->timestamp.tv_sec = tmp.sec;
	ans->timestamp.tv_nsec = tmp.nsec;
	*result = ans;
    }
    return sts;
}

int
__pmFetchArchive(__pmContext *ctxp, __pmResult **result)
{
    int		sts;

    if ((sts = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(sts);
	if (ctxp == NULL)
	    sts = PM_ERR_NOCONTEXT;
	else {
	    if (ctxp->c_type != PM_CONTEXT_ARCHIVE)
		sts = PM_ERR_NOTARCHIVE;
	    else if (ctxp->c_mode == PM_MODE_INTERP)
		/* makes no sense! */
		sts = PM_ERR_MODE;
	    else {
		/* assume PM_CONTEXT_ARCHIVE and BACK or FORW */
		sts = __pmLogFetch(ctxp, 0, NULL, result);
		if (sts >= 0)
		    ctxp->c_origin = (*result)->timestamp;
	    }
	    PM_UNLOCK(ctxp->c_lock);
	}
    }

    return sts;
}

int
pmFetchArchive_v2(pmResult_v2 **result)
{
    __pmResult	*rp;
    int		sts;

    if ((sts = __pmFetchArchive(NULL, &rp)) >= 0) {
	pmResult_v2	*ans = __pmOffsetResult_v2(rp);
	__pmTimestamp	tmp = rp->timestamp;	/* struct copy */

	ans->timestamp.tv_sec = tmp.sec;
	ans->timestamp.tv_usec = tmp.nsec / 1000;
	*result = ans;
    }
    return sts;
}

int
pmFetchArchive(pmResult **result)
{
    __pmResult	*rp;
    int		sts;

    if ((sts = __pmFetchArchive(NULL, &rp)) >= 0) {
	pmResult	*ans = __pmOffsetResult(rp);
	__pmTimestamp	tmp = rp->timestamp;	/* struct copy */

	ans->timestamp.tv_sec = tmp.sec;
	ans->timestamp.tv_nsec = tmp.nsec;
	*result = ans;
    }
    return sts;
}

int
__pmSetMode(int mode, const __pmTimestamp *when, const __pmTimestamp *delta, int direction)
{
    int		sts;
    __pmContext	*ctxp;

    if ((sts = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(sts);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if (mode != PM_MODE_LIVE)
		sts = PM_ERR_MODE;
	    else {
		ctxp->c_origin.sec = ctxp->c_origin.nsec = 0;
		ctxp->c_mode = mode;
		ctxp->c_delta = *delta;
		ctxp->c_direction = direction;
		sts = 0;
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    sts = PM_ERR_MODE;
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    if (mode == PM_MODE_INTERP ||
		mode == PM_MODE_FORW || mode == PM_MODE_BACK) {
		int	lsts;
		if (when != NULL) {
		    /*
		     * special case of NULL for timestamp
		     * => do not update notion of "current" time
		     */
		    ctxp->c_origin = *when;
		}
		ctxp->c_mode = mode;
		ctxp->c_delta = *delta;
		ctxp->c_direction = direction;
		lsts = __pmLogSetTime(ctxp);
		if (lsts < 0) {
		    /*
		     * most unlikely; not much we can do here but expect
		     * PMAPI error to be returned once pmFetch's start
		     */
		    if (pmDebugOptions.log) {
			char	errmsg[PM_MAXERRMSGLEN];
			fprintf(stderr, "%s: %s failed: %s\n",
				"pmSetMode", "__pmLogSetTime",
				pmErrStr_r(lsts, errmsg, sizeof(errmsg)));
		    }
		}
		__pmLogResetInterp(ctxp);
		sts = 0;
	    }
	    else
		sts = PM_ERR_MODE;
	}
	PM_UNLOCK(ctxp->c_lock);
    }

    return sts;
}

#ifndef PM_XTB_FLAG
/*
 * Extended time base definitions and macros ...
 * were in pmapi.h, but deprecated there so need 'em here for
 * backwards API compatibility
 */
#define PM_XTB_FLAG	0x1000000
#define PM_XTB_SET(m)	(PM_XTB_FLAG | ((m) << 16))
#define PM_XTB_GET(m)	(((m) & PM_XTB_FLAG) ? (((m) & 0xff0000) >> 16) : -1)
#endif

int
pmSetMode_v2(int mode, const struct timeval *when, int delta)
{
    __pmTimestamp	offset, interval;
    int			direction;

    if (delta < 0)
	direction = -1;
    else if (delta > 0)
	direction = 1;
    else
	direction = 0;

    /* convert milliseconds (with optional extended time base) to sec/nsec */
    switch(PM_XTB_GET(mode)) {
    case PM_TIME_NSEC:
	interval.sec = 0;
	interval.nsec = delta;
	break;
    case PM_TIME_USEC:
	interval.sec = 0;
	interval.nsec = delta * 1000;
	break;
    case PM_TIME_SEC:
	interval.sec = delta;
	interval.nsec = 0;
	break;
    case PM_TIME_MIN:
	interval.sec = delta * 60;
	interval.nsec = 0;
	break;
    case PM_TIME_HOUR:
	interval.sec = delta * 360;
	interval.nsec = 0;
	break;
    case PM_TIME_MSEC:
    default:
	interval.sec = delta / 1000;
	interval.nsec = 1000000 * (delta % 1000);
	break;
    }

    if (when == NULL)
	return __pmSetMode(mode, NULL, &interval, direction);

    offset.sec = when->tv_sec;
    offset.nsec = when->tv_usec * 1000;
    return __pmSetMode(mode, &offset, &interval, direction);
}

int
pmSetMode(int mode, const struct timespec *when, const struct timespec *delta)
{
    __pmTimestamp	offset, interval;
    int			direction;
    int			sts;

    /* set internal delta time */
    if (delta != NULL) {
	interval.sec = delta->tv_sec;
	interval.nsec = delta->tv_nsec;
    } else {
	interval.sec = interval.nsec = 0;
    }

    /* set internal delta direction */
    if (delta == NULL)
	direction = 0;
    else if (delta->tv_sec < 0 || delta->tv_nsec < 0)
	direction = -1;
    else if (delta->tv_sec || delta->tv_nsec)
	direction = 1;
    else
	direction = 0;

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, "pmSetMode(mode=");
	switch (mode) {
	    case PM_MODE_LIVE:
		fprintf(stderr, "LIVE");
		break;
	    case PM_MODE_INTERP:
		fprintf(stderr, "INTERP");
		break;
	    case PM_MODE_FORW:
		fprintf(stderr, "FORW");
		break;
	    case PM_MODE_BACK:
		fprintf(stderr, "BACK");
		break;
	    default:
		fprintf(stderr, "%d?", mode);
		break;
	}
	fprintf(stderr, ", when=");
	pmtimespecPrint(stderr, when);
	fprintf(stderr, ", delta=");
	pmtimespecPrintInterval(stderr, delta);
	fprintf(stderr, ") direction=%d <:", direction);
    }

    if (when == NULL) {
	sts =  __pmSetMode(mode, NULL, &interval, direction);
	goto pmapi_return;
    }
    offset.sec = when->tv_sec;
    offset.nsec = when->tv_nsec;
    sts = __pmSetMode(mode, &offset, &interval, direction);

pmapi_return:

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, ":> returns ");
	if (sts >= 0)
	    fprintf(stderr, "%d\n", sts);
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }

    return sts;
}
