/*
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Thread-safe note
 *
 * myFetch() returns a PDU buffer that is pinned from _pmGetPDU() or
 * __pmEncodeResult() and this needs to be unpinned by the myFetch()
 * caller when safe to do so.
 */

#include "logger.h"

static int
myLocalFetch(__pmContext *ctxp, int numpmid, pmID pmidlist[], __pmPDU **pdup)
{
    pmResult	*result;
    pmID	*newlist = NULL;
    int		newcnt, have_dm, n;

    /* for derived metrics, may need to rewrite the pmidlist */
    have_dm = newcnt = __pmPrepareFetch(ctxp, numpmid, pmidlist, &newlist);
    if (newcnt > numpmid) {
	/* replace args passed into myFetch */
	numpmid = newcnt;
	pmidlist = newlist;
    }

    if ((n = __pmFetchLocal(ctxp, numpmid, pmidlist, &result)) < 0) {
	if (newlist != NULL)
	    free(newlist);
	return n;
    }

    /* process derived metrics, if any */
    if (have_dm) {
	__pmFinishResult(ctxp, n, &result);
	if (newlist != NULL)
	    free(newlist);
    }

    return __pmEncodeResult(0, result, pdup);
}

int
myFetch(int numpmid, pmID pmidlist[], __pmPDU **pdup)
{
    int			n = 0;
    int			ctx;
    __pmPDU		*pb;
    __pmContext		*ctxp;

    if (numpmid < 1)
	return PM_ERR_TOOSMALL;

    if ((ctx = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	if (ctxp->c_type != PM_CONTEXT_HOST) {
	    if (ctxp->c_type == PM_CONTEXT_LOCAL)
		n = myLocalFetch(ctxp, numpmid, pmidlist, pdup);
	    else
		n = PM_ERR_NOTHOST;
	    PM_UNLOCK(ctxp->c_lock);
	    return n;
	}
    }
    else
	return PM_ERR_NOCONTEXT;

#if CAN_RECONNECT
    if (ctxp->c_pmcd->pc_fd == -1) {
	/* lost connection, try to get it back */
	n = reconnect();
	if (n < 0) {
	    PM_UNLOCK(ctxp->c_lock);
	    return n;
	}
    }
#endif

    if (ctxp->c_sent == 0) {
	/*
	 * current profile is _not_ already cached at other end of
	 * IPC, so send current profile
	 */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PROFILE)
	    fprintf(stderr, "myFetch: calling __pmSendProfile, context: %d\n", ctx);
#endif
	if ((n = __pmSendProfile(ctxp->c_pmcd->pc_fd, FROM_ANON, ctx, ctxp->c_instprof)) >= 0)
	    ctxp->c_sent = 1;
    }

    if (n >= 0) {
	int		newcnt;
	pmID		*newlist = NULL;
	int		have_dm;

	/* for derived metrics, may need to rewrite the pmidlist */
	have_dm = newcnt = __pmPrepareFetch(ctxp, numpmid, pmidlist, &newlist);
	if (newcnt > numpmid) {
	    /* replace args passed into myFetch */
	    numpmid = newcnt;
	    pmidlist = newlist;
	}

	n = __pmSendFetch(ctxp->c_pmcd->pc_fd, FROM_ANON, ctx, &ctxp->c_origin, numpmid, pmidlist);
	if (n >= 0){
	    int		changed = 0;
	    do {
		n = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb);
		/*
		 * expect PDU_RESULT or
		 *        PDU_ERROR(changed > 0)+PDU_RESULT or
		 *        PDU_ERROR(real error < 0 from PMCD) or
		 *        0 (end of file)
		 *        < 0 (local error or IPC problem)
		 *        other (bogus PDU)
		 */
		if (n == PDU_RESULT) {
		    /*
		     * Success with a pmResult in a pdubuf.
		     *
		     * Need to process derived metrics, if any.
		     * This is ugly, we need to decode the pdubuf, rebuild
		     * the pmResult and encode back into a pdubuf ... the
		     * fastpath of not doing all of this needs to be
		     * preserved in the common case where derived metrics
		     * are not being logged.
		     */
		    if (have_dm) {
			pmResult	*result;
			__pmPDU		*npb;
			int		sts;

			if ((sts = __pmDecodeResult(pb, &result)) < 0) {
			    n = sts;
			}
			else {
			    __pmFinishResult(ctxp, sts, &result);
			    if ((sts = __pmEncodeResult(ctxp->c_pmcd->pc_fd, result, &npb)) < 0)
				n = sts;
			    else {
				/* using PDU with derived metrics */
				__pmUnpinPDUBuf(pb);
				*pdup = npb;
			    }
			}
		    }
		    else
			*pdup = pb;
		}
		else if (n == PDU_ERROR) {
		    __pmDecodeError(pb, &n);
		    if (n > 0) {
			/* PMCD state change protocol */
			changed = n;
			n = 0;
		    }
		    else {
			fprintf(stderr, "myFetch: ERROR PDU: %s\n", pmErrStr(n));
			disconnect(PM_ERR_IPC);
		    }
		    __pmUnpinPDUBuf(pb);
		}
		else if (n == 0) {
		    fprintf(stderr, "myFetch: End of File: PMCD exited?\n");
		    disconnect(PM_ERR_IPC);
		}
		else if (n < 0) {
		    fprintf(stderr, "myFetch: __pmGetPDU: Error: %s\n", pmErrStr(n));
		    disconnect(PM_ERR_IPC);
		}
		else {
		    fprintf(stderr, "myFetch: Unexpected %s PDU from PMCD\n", __pmPDUTypeStr(n));
		    disconnect(PM_ERR_IPC);
		    __pmUnpinPDUBuf(pb);
		}
	    } while (n == 0);

	    if (changed & PMCD_ADD_AGENT) {
		/*
		 * PMCD_DROP_AGENT does not matter, no values are returned.
		 * Trying to restart (PMCD_RESTART_AGENT) is less interesting
		 * than when we actually start (PMCD_ADD_AGENT) ... the latter
		 * is also set when a successful restart occurs, but more
		 * to the point the sequence Install-Remove-Install does
		 * not involve a restart ... it is the second Install that
		 * generates the second PMCD_ADD_AGENT that we need to be
		 * particularly sensitive to, as this may reset counter
		 * metrics ...
		 */
		int	sts;
		if ((sts = putmark()) < 0) {
		    fprintf(stderr, "putmark: %s\n", pmErrStr(sts));
		    exit(1);
		}
	    }
	}
	if (newlist != NULL)
	    free(newlist);
    }

    if (n < 0 && ctxp->c_pmcd->pc_fd != -1) {
	disconnect(n);
    }

    PM_UNLOCK(ctxp->c_lock);
    return n;
}
