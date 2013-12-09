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

static int
request_fetch(int ctxid, __pmContext *ctxp,  int numpmid, pmID pmidlist[])
{
    int n;

    if (ctxp->c_sent == 0) {
	/*
	 * current profile is _not_ already cached at other end of
	 * IPC, so send get current profile
	 */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PROFILE) {
	    fprintf(stderr, "pmFetch: calling __pmSendProfile, context: %d\n",
	            ctxid);
	    __pmDumpProfile(stderr, PM_INDOM_NULL, ctxp->c_instprof);
	}
#endif
	if ((n = __pmSendProfile(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp),
			ctxid, ctxp->c_instprof)) < 0)
	    return (__pmMapErrno(n));
	else
	    ctxp->c_sent = 1;
    }

    n = __pmSendFetch(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp), ctxid, 
		      &ctxp->c_origin, numpmid, pmidlist);
    if (n < 0) {
	    n = __pmMapErrno(n);
    }
    return n;
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

int
pmFetch(int numpmid, pmID pmidlist[], pmResult **result)
{
    int		n;

    if (numpmid < 1) {
	n = PM_ERR_TOOSMALL;
	goto done;
    }

    if ((n = pmWhichContext()) >= 0) {
	__pmContext	*ctxp = __pmHandleToPtr(n);
	int		newcnt;
	pmID		*newlist = NULL;
	int		have_dm;

	if (ctxp == NULL) {
	    n = PM_ERR_NOCONTEXT;
	    goto done;
	}
	if (ctxp->c_type == PM_CONTEXT_LOCAL && PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA)) {
	    /* Local context requires single-threaded applications */
	    n = PM_ERR_THREAD;
	    PM_UNLOCK(ctxp->c_lock);
	    goto done;
	}

	/* for derived metrics, may need to rewrite the pmidlist */
	have_dm = newcnt = __pmPrepareFetch(ctxp, numpmid, pmidlist, &newlist);
	if (newcnt > numpmid) {
	    /* replace args passed into pmFetch */
	    numpmid = newcnt;
	    pmidlist = newlist;
	}

	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    /*
	     * Thread-safe note
	     *
	     * Need to be careful here, because the PMCD changed protocol
	     * may mean several PDUs are returned, but __pmDecodeResult()
	     * may request more info from PMCD if pmDebug is set.
	     *
	     * So unlock ctxp->c_pmcd->pc_lock as soon as possible.
	     */
	    PM_LOCK(ctxp->c_pmcd->pc_lock);
	    if ((n = request_fetch(n, ctxp, numpmid, pmidlist)) >= 0) {
		int changed = 0;
		do {
		    __pmPDU	*pb;
		    int		pinpdu;

		    pinpdu = n = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE,
					    ctxp->c_pmcd->pc_tout_sec, &pb);
		    if (n == PDU_RESULT) {
			PM_UNLOCK(ctxp->c_pmcd->pc_lock);
			n = __pmDecodeResult(pb, result);
		    }
		    else if (n == PDU_ERROR) {
			__pmDecodeError(pb, &n);
			if (n > 0)
			    /* PMCD state change protocol */
			    changed = n;
			else
			    PM_UNLOCK(ctxp->c_pmcd->pc_lock);
		    }
		    else {
			PM_UNLOCK(ctxp->c_pmcd->pc_lock);
			if (n != PM_ERR_TIMEOUT)
			    n = PM_ERR_IPC;
		    }
		    if (pinpdu > 0)
			__pmUnpinPDUBuf(pb);
		} while (n > 0);

		if (n == 0)
		    n |= changed;
	    }
	    else
		PM_UNLOCK(ctxp->c_pmcd->pc_lock);
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    n = __pmFetchLocal(ctxp, numpmid, pmidlist, result);
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    n = __pmLogFetch(ctxp, numpmid, pmidlist, result);
	    if (n >= 0 && (ctxp->c_mode & __PM_MODE_MASK) != PM_MODE_INTERP) {
		ctxp->c_origin.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
		ctxp->c_origin.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
	    }
	}

	/* process derived metrics, if any */
	if (have_dm) {
	    __pmFinishResult(ctxp, n, result);
	    if (newlist != NULL)
		free(newlist);
	}
	PM_UNLOCK(ctxp->c_lock);
    }

done:
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_FETCH) {
	fprintf(stderr, "pmFetch returns ...\n");
	if (n > 0) {
	    fprintf(stderr, "PMCD state changes: agent(s)");
	    if (n & PMCD_ADD_AGENT) fprintf(stderr, " added");
	    if (n & PMCD_RESTART_AGENT) fprintf(stderr, " restarted");
	    if (n & PMCD_DROP_AGENT) fprintf(stderr, " dropped");
	    fputc('\n', stderr);
	}
	if (n >= 0)
	    __pmDumpResult(stderr, *result);
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "Error: %s\n", pmErrStr_r(n, errmsg, sizeof(errmsg)));
	}
    }
#endif

    return n;
}

int
pmFetchArchive(pmResult **result)
{
    int		n;
    __pmContext	*ctxp;
    int		ctxp_mode;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp == NULL)
	    n = PM_ERR_NOCONTEXT;
	else {
	    ctxp_mode = (ctxp->c_mode & __PM_MODE_MASK);
	    if (ctxp->c_type != PM_CONTEXT_ARCHIVE)
		n = PM_ERR_NOTARCHIVE;
	    else if (ctxp_mode == PM_MODE_INTERP)
		/* makes no sense! */
		n = PM_ERR_MODE;
	    else {
		/* assume PM_CONTEXT_ARCHIVE and BACK or FORW */
		n = __pmLogFetch(ctxp, 0, NULL, result);
		if (n >= 0) {
		    ctxp->c_origin.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
		    ctxp->c_origin.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
		}
	    }
	    PM_UNLOCK(ctxp->c_lock);
	}
    }

    return n;
}

int
pmSetMode(int mode, const struct timeval *when, int delta)
{
    int		n;
    __pmContext	*ctxp;
    int		l_mode = (mode & __PM_MODE_MASK);

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if (l_mode != PM_MODE_LIVE)
		n = PM_ERR_MODE;
	    else {
		ctxp->c_origin.tv_sec = ctxp->c_origin.tv_usec = 0;
		ctxp->c_mode = mode;
		ctxp->c_delta = delta;
		n = 0;
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    n = PM_ERR_MODE;
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
		n = 0;
	    }
	    else
		n = PM_ERR_MODE;
	}
	PM_UNLOCK(ctxp->c_lock);
    }
    return n;
}
