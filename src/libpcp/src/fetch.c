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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include "pmapi.h"
#include "impl.h"

extern int __pmFetchLocal(int, pmID *, pmResult **);

static int
request_fetch (int ctxid, __pmContext *ctxp,  int numpmid, pmID pmidlist[])
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0) {
	return (PM_ERR_CTXBUSY);
    }

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
	if ((n = __pmSendProfile(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
				 ctxid, ctxp->c_instprof)) < 0)
	    return (__pmMapErrno(n));
	else
	    ctxp->c_sent = 1;
    }

    n = __pmSendFetch(ctxp->c_pmcd->pc_fd, PDU_BINARY, ctxid, 
		      &ctxp->c_origin, numpmid, pmidlist);
    if (n < 0) {
	    n = __pmMapErrno(n);
    }
    return (n);
}

int 
pmRequestFetch(int ctxid, int numpmid, pmID pmidlist[])
{
    int n = 0;
    __pmContext *ctxp;

    if ((n = __pmGetHostContextByID(ctxid, &ctxp)) >= 0) {
	if ((n = request_fetch (ctxid, ctxp, numpmid, pmidlist)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_FETCH;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }
    return (n);
}


static int
receive_fetch (__pmContext *ctxp, pmResult **result)
{
    int n;
    __pmPDU	*pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY,
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_RESULT) {
	n = __pmDecodeResult(pb, PDU_BINARY, result);
    }
    else if (n == PDU_ERROR) {
	__pmDecodeError(pb, PDU_BINARY, &n);
    }
    else if (n != PM_ERR_TIMEOUT)
	n = PM_ERR_IPC;

    return (n);
}

int
pmReceiveFetch (int ctxid, pmResult **result)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetBusyHostContextByID (ctxid, &ctxp, PDU_FETCH)) >= 0) {
	if ((n = receive_fetch (ctxp, result)) <= 0) {
	    /* pmcd may return state change in error PDU before
	     * returning results for fetch - if we get one of those,
	     * keep the state in the context for the second call to
	     * receive_fetch */
	    ctxp->c_pmcd->pc_curpdu = 0;
	    ctxp->c_pmcd->pc_tout_sec = 0;
	}
    }
    return (n);
}

int
pmFetch(int numpmid, pmID pmidlist[], pmResult **result)
{
    int		n;

    if (numpmid < 1)
	return PM_ERR_TOOSMALL;

    if ((n = pmWhichContext()) >= 0) {
	__pmContext *ctxp = __pmHandleToPtr(n);

	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if ((n = request_fetch (n, ctxp, numpmid, pmidlist)) >= 0) {
		int changed = 0;
		do {
		    if ((n = receive_fetch (ctxp, result)) > 0) {
			/* PMCD state change protocol */
			changed = n;
		    }
		} while (n > 0);

		if (n == 0)
		    n |= changed;
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    n = __pmFetchLocal(numpmid, pmidlist, result);
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    n = __pmLogFetch(ctxp, numpmid, pmidlist, result);
	    if (n >= 0 && (ctxp->c_mode & __PM_MODE_MASK) != PM_MODE_INTERP) {
		ctxp->c_origin.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
		ctxp->c_origin.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
	    }
	}
    }

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
	else
	    fprintf(stderr, "Error: %s\n", pmErrStr(n));
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
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if (l_mode != PM_MODE_LIVE)
		return PM_ERR_MODE;

	    ctxp->c_origin.tv_sec = ctxp->c_origin.tv_usec = 0;
	    ctxp->c_mode = mode;
	    ctxp->c_delta = delta;
	    return 0;
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
		return PM_ERR_MODE;
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
		return 0;
	    }
	    else
		return PM_ERR_MODE;
	}
    }
    else
	return n;
}
