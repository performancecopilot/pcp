/*
 * Copyright (c) 1995-2006,2008 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "impl.h"
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
__pmRecvFetch(int fd, __pmContext *ctxp, int timeout, pmResult **result)
{
    __pmPDU	*pb;
    int		sts, pinpdu, changed = 0;

    do {
	sts = pinpdu = __pmGetPDU(fd, ANY_SIZE, timeout, &pb);
	if (sts == PDU_RESULT) {
	    sts = __pmDecodeResult_ctx(ctxp, pb, result);
	}
	else if (sts == PDU_ERROR) {
	    __pmDecodeError(pb, &sts);
	    if (sts > 0)
		/* PMCD state change protocol */
		changed |= sts;
	}
	else if (sts != PM_ERR_TIMEOUT)
	    sts = PM_ERR_IPC;

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
__pmFinishResult(__pmContext *ctxp, int count, pmResult **resultp)
{
    if (count >= 0)
	__dmpostfetch(ctxp, resultp);
    return count;
}

/*
 * Internal variant of pmFetch() ... ctxp is not NULL for
 * internal callers where the current context is already locked, but
 * NULL for callers from above the PMAPI or internal callers when the
 * current context is not locked.
 */
int
pmFetch_ctx(__pmContext *ctxp, int numpmid, pmID *pmidlist, pmResult **result)
{
    int		need_unlock = 0;
    int		fd, ctx, sts, tout;

    if (pmDebugOptions.pmapi) {
	char    dbgbuf[20];
	fprintf(stderr, "pmFetch(%d, pmid[0] %s", numpmid, pmIDStr_r(pmidlist[0], dbgbuf, sizeof(dbgbuf)));
	if (numpmid > 1)
	    fprintf(stderr, " ... pmid[%d] %s", numpmid-1, pmIDStr_r(pmidlist[numpmid-1], dbgbuf, sizeof(dbgbuf)));
	fprintf(stderr, ", ...) <:");
    }

    if (numpmid < 1) {
	sts = PM_ERR_TOOSMALL;
	goto pmapi_return;
    }

    if ((sts = ctx = pmWhichContext()) >= 0) {
	int		newcnt;
	pmID		*newlist = NULL;
	int		have_dm;

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
	if (ctxp->c_type == PM_CONTEXT_LOCAL && PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA)) {
	    /* Local context requires single-threaded applications */
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
	    tout = ctxp->c_pmcd->pc_tout_sec;
	    fd = ctxp->c_pmcd->pc_fd;
	    if ((sts = __pmUpdateProfile(fd, ctxp, tout)) < 0) {
		sts = __pmMapErrno(sts);
	    }
	    else if ((sts = __pmSendFetch(fd, __pmPtrToHandle(ctxp), ctxp->c_slot,
				&ctxp->c_origin, numpmid, pmidlist)) < 0) {
		sts = __pmMapErrno(sts);
	    }
	    else {
		PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_TIMEOUT);
		sts = __pmRecvFetch(fd, ctxp, tout, result);
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    sts = __pmFetchLocal(ctxp, numpmid, pmidlist, result);
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    sts = __pmLogFetch(ctxp, numpmid, pmidlist, result);
	    if (sts >= 0 && (ctxp->c_mode & __PM_MODE_MASK) != PM_MODE_INTERP) {
		ctxp->c_origin.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
		ctxp->c_origin.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
	    }
	}

	/* process derived metrics, if any */
	if (have_dm) {
	    __pmFinishResult(ctxp, sts, result);
	    if (newlist != NULL)
		free(newlist);
	}
    }

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

    if (pmDebugOptions.fetch) {
	fprintf(stderr, "pmFetch returns ...\n");
	if (sts > 0) {
	    fprintf(stderr, "PMCD state changes: agent(s)");
	    if (sts & PMCD_ADD_AGENT) fprintf(stderr, " added");
	    if (sts & PMCD_RESTART_AGENT) fprintf(stderr, " restarted");
	    if (sts & PMCD_DROP_AGENT) fprintf(stderr, " dropped");
	    fputc('\n', stderr);
	}
	if (sts >= 0)
	    __pmDumpResult_ctx(ctxp, stderr, *result);
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "Error: %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }
    if (need_unlock) {
	PM_UNLOCK(ctxp->c_lock);
    }

    return sts;
}

int
pmFetch(int numpmid, pmID *pmidlist, pmResult **result)
{
    int	sts;
    sts = pmFetch_ctx(NULL, numpmid, pmidlist, result);
    return sts;
}

int
pmFetchArchive(pmResult **result)
{
    int		sts;
    __pmContext	*ctxp;
    int		ctxp_mode;

    if ((sts = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(sts);
	if (ctxp == NULL)
	    sts = PM_ERR_NOCONTEXT;
	else {
	    ctxp_mode = (ctxp->c_mode & __PM_MODE_MASK);
	    if (ctxp->c_type != PM_CONTEXT_ARCHIVE)
		sts = PM_ERR_NOTARCHIVE;
	    else if (ctxp_mode == PM_MODE_INTERP)
		/* makes no sense! */
		sts = PM_ERR_MODE;
	    else {
		/* assume PM_CONTEXT_ARCHIVE and BACK or FORW */
		if ((sts = __pmLogFetch(ctxp, 0, NULL, result)) >= 0) {
		    ctxp->c_origin.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
		    ctxp->c_origin.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
		}
	    }
	    PM_UNLOCK(ctxp->c_lock);
	}
    }

    return sts;
}

int
pmSetMode(int mode, const struct timeval *when, int delta)
{
    int		sts;
    __pmContext	*ctxp;
    int		l_mode = (mode & __PM_MODE_MASK);

    if ((sts = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(sts);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if (l_mode != PM_MODE_LIVE)
		sts = PM_ERR_MODE;
	    else {
		ctxp->c_origin.tv_sec = ctxp->c_origin.tv_usec = 0;
		ctxp->c_mode = mode;
		ctxp->c_delta = delta;
		sts = 0;
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    sts = PM_ERR_MODE;
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    if (l_mode == PM_MODE_INTERP ||
		l_mode == PM_MODE_FORW || l_mode == PM_MODE_BACK) {
		if (when != NULL) {
		    /*
		     * special case of NULL for timestamp
		     * => do not update notion of "current" time
		     */
		    ctxp->c_origin.tv_sec = (__int32_t)when->tv_sec;
		    ctxp->c_origin.tv_usec = (__int32_t)when->tv_usec;
		}
		ctxp->c_mode = mode;
		ctxp->c_delta = delta;
		__pmLogSetTime(ctxp);
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
