/*
 * Copyright (c) 2013-2018 Red Hat.
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
    int		sts;

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

    sts = __pmEncodeResult(0, result, pdup);
    pmFreeResult(result);

    return sts;
}

/*
 * from libpcp/src/p_result.c ... used for -Dfetch output
 */
typedef struct {
    pmID		pmid;
    int			numval;		/* no. of vlist els to follow, or error */
    int			valfmt;		/* insitu or pointer */
    __pmValue_PDU	vlist[1];	/* zero or more */
} vlist_t;
typedef struct {
    __pmPDUHdr		hdr;
    pmTimeval		timestamp;	/* when returned */
    int			numpmid;	/* no. of PMIDs to follow */
    __pmPDU		data[1];	/* zero or more */
} result_t;
/*
 * from libpcp/src/internal.h ... used for -Dfetch output
 */
#ifdef HAVE_NETWORK_BYTEORDER
#define __ntohpmID(a)           (a)
#else
#define __ntohpmID(a)           htonl(a)
#endif

int
myFetch(int numpmid, pmID pmidlist[], __pmPDU **pdup)
{
    int			n = 0;
    int			changed = 0;
    int			ctx;
    __pmPDU		*pb;
    __pmContext		*ctxp;

    if (numpmid < 1)
	return PM_ERR_TOOSMALL;

    if ((ctx = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	/*
	 * Note: This application is single threaded, and once we have ctxp
	 *	 the associated __pmContext will not move and will only be
	 *	 accessed or modified synchronously either here or in libpcp.
	 *	 We unlock the context so that it can be locked as required
	 *	 within libpcp.
	 */
	PM_UNLOCK(ctxp->c_lock);
	if (ctxp->c_type != PM_CONTEXT_HOST) {
	    if (ctxp->c_type == PM_CONTEXT_LOCAL)
		n = myLocalFetch(ctxp, numpmid, pmidlist, pdup);
	    else
		n = PM_ERR_NOTHOST;
	    return n;
	}
    }
    else
	return PM_ERR_NOCONTEXT;

    if (ctxp->c_pmcd->pc_fd == -1) {
	/* lost connection, try to get it back */
	n = reconnect();
	if (n < 0) {
	    return n;
	}
    }

    if (ctxp->c_sent == 0) {
	/*
	 * current profile is _not_ already cached at other end of
	 * IPC, so send current profile
	 */
	if (pmDebugOptions.profile)
	    fprintf(stderr, "myFetch: calling __pmSendProfile, context: %d\n", ctx);
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

	n = __pmSendFetch(ctxp->c_pmcd->pc_fd, FROM_ANON, ctx, numpmid, pmidlist);
	if (n >= 0) {
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

		if (pmDebugOptions.fetch) {
		    fprintf(stderr, "myFetch returns ...\n");
		    if (n == PDU_ERROR) {
			int		sts;
			int		flag = 0;

			__pmDecodeError(pb, &sts);
			fprintf(stderr, "PMCD state changes: ");
			if (sts & PMCD_AGENT_CHANGE) {
			    fprintf(stderr, "agent(s)");
			    if (sts & PMCD_ADD_AGENT) fprintf(stderr, " added");
			    if (sts & PMCD_RESTART_AGENT) fprintf(stderr, " restarted");
			    if (sts & PMCD_DROP_AGENT) fprintf(stderr, " dropped");
			    flag++;
			}
			if (sts & PMCD_LABEL_CHANGE) {
			    if (flag++)
				fprintf(stderr, ", ");
			    fprintf(stderr, "label change");
			}
			if (sts & PMCD_NAMES_CHANGE) {
			    if (flag++)
				fprintf(stderr, ", ");
			    fprintf(stderr, "names change");
			}
			fputc('\n', stderr);
		    }
		    else if (n == PDU_RESULT) {
			/*
			 * not safe to decode result here, so have to make
			 * do with a shallow dump of the PDU using logic
			 * from libpcp/__pmDecodeResult()
			 */
			int		numpmid;
			int		numval;
			int		i;
			int		vsize;
			pmID		pmid;
			struct timeval	timestamp;
			char		*name;
			result_t	*pp;
			vlist_t		*vlp;
			pp = (result_t *)pb;
			/* assume PDU is valid ... it comes from pmcd */
			numpmid = ntohl(pp->numpmid);
			timestamp.tv_sec = ntohl(pp->timestamp.tv_sec);
			timestamp.tv_usec = ntohl(pp->timestamp.tv_usec);
			fprintf(stderr, "pmResult timestamp: %d.%06d numpmid: %d\n", (int)timestamp.tv_sec, (int)timestamp.tv_usec, numpmid);
			vsize = 0;
			for (i = 0; i < numpmid; i++) {
			    vlp = (vlist_t *)&pp->data[vsize/sizeof(__pmPDU)];
			    pmid = __ntohpmID(vlp->pmid);
			    numval = ntohl(vlp->numval);
			    fprintf(stderr, "  %s", pmIDStr(pmid));
			    if (pmNameID(pmid, &name) == 0) {
				fprintf(stderr, " (%s)", name);
				free(name);
			    }
			    fprintf(stderr, ": numval: %d", numval);
			    if (numval > 0)
				fprintf(stderr, " valfmt: %d", ntohl(vlp->valfmt));
			    fputc('\n', stderr);
			    vsize += sizeof(vlp->pmid) + sizeof(vlp->numval);
			    if (numval > 0)
				vsize += sizeof(vlp->valfmt) + ntohl(vlp->numval) * sizeof(__pmValue_PDU);
			}
		    }
		    else
			fprintf(stderr, "__pmGetPDU: Error: %s\n", pmErrStr(n));
		}

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
			    if ((sts = __pmEncodeResult(ctxp->c_pmcd->pc_fd, result, &npb)) < 0) {
				pmFreeResult(result);
				n = sts;
			    }
			    else {
				/* using PDU with derived metrics */
				__pmUnpinPDUBuf(pb);
				*pdup = npb;
				pmFreeResult(result);
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
			changed = 0;
		    }
		    __pmUnpinPDUBuf(pb);
		}
		else if (n == 0) {
		    fprintf(stderr, "myFetch: End of File: PMCD exited?\n");
		    disconnect(PM_ERR_IPC);
		    changed = 0;
		}
		else if (n == -EINTR) {
		    /* SIGINT, let the normal cleanup happen */
		    ;
		}
		else if (n < 0) {
		    /* other badness, disconnect */
		    fprintf(stderr, "myFetch: __pmGetPDU: Error: %s\n", pmErrStr(n));
		    disconnect(PM_ERR_IPC);
		    changed = 0;
		}
		else {
		    /* protocol botch, disconnect */
		    fprintf(stderr, "myFetch: Unexpected %s PDU from PMCD\n", __pmPDUTypeStr(n));
		    disconnect(PM_ERR_IPC);
		    changed = 0;
		    __pmUnpinPDUBuf(pb);
		}
	    } while (n == 0);

	    if (changed & PMCD_NAMES_CHANGE) {
		/*
		 * Fetch has returned with the PMCD_NAMES_CHANGE flag set.
		 */
		check_dynamic_metrics();
	    }

	    if (changed & PMCD_ADD_AGENT) {
		int	sts;
		/*
		 * PMCD_DROP_AGENT does not matter, no values are returned.
		 * Trying to restart (PMCD_RESTART_AGENT) is less interesting
		 * than when we actually start (PMCD_ADD_AGENT) ... the latter
		 * is also set when a successful restart occurs, but more
		 * to the point the sequence Install-Remove-Install does
		 * not involve a restart ... it is the second Install that
		 * generates the second PMCD_ADD_AGENT that we need to be
		 * particularly sensitive to, as this may reset counter
		 * metrics.
		 *
		 * The potentially new instance of the agent may also be an
		 * updated one, so it's PMNS could have changed. We need to
		 * recheck each metric to make sure that its pmid and semantics
		 * have not changed.
		 * This call will not return if there is an incompatible change.
		 */
		validate_metrics();

		if (changed & PMCD_ADD_AGENT) {
		     /*
		      * All metrics have been validated, however, the state change
		      * PMCD_ADD_AGENT represents a potential gap in the stream of
		      * metrics. So we generate a <mark> record for this case.
		      */
		    if ((sts = putmark()) < 0) {
			fprintf(stderr, "putmark: %s\n", pmErrStr(sts));
			exit(1);
		    }
		}
	    }
	}
	else {
	    fprintf(stderr, "Error: __pmSendFetch: %s\n", pmErrStr(n));
	}
	if (newlist != NULL)
	    free(newlist);
    }

    if (n < 0) {
	if (ctxp->c_pmcd->pc_fd != -1)
	    disconnect(n);
	return n;
    }

    return changed;
}
