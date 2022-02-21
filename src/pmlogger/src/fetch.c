/*
 * Copyright (c) 2013-2018,2022 Red Hat.
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
 */

#include "logger.h"

static int
myLocalFetch(__pmContext *ctxp, int numpmid, pmID pmidlist[], __pmResult **result)
{
    pmID	*newlist = NULL;
    int		newcnt, have_dm, n;

    /* for derived metrics, may need to rewrite the pmidlist */
    have_dm = newcnt = __pmPrepareFetch(ctxp, numpmid, pmidlist, &newlist);
    if (newcnt > numpmid) {
	/* replace args passed into myFetch */
	numpmid = newcnt;
	pmidlist = newlist;
    }

    if ((n = __pmFetchLocal(ctxp, numpmid, pmidlist, result)) < 0) {
	if (newlist != NULL)
	    free(newlist);
	return n;
    }

    /* process derived metrics, if any */
    if (have_dm) {
	__pmFinishResult(ctxp, n, result);
	if (newlist != NULL)
	    free(newlist);
    }

    return 0;
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
typedef struct {
    __pmPDUHdr		hdr;		/* three word (3x int) PDU header */
    int			numpmid;	/* count of PMIDs to follow */
    pmTimespec		timestamp;	/* 64bit-aligned time */
    __pmPDU		data[2];	/* zero or more (2 for alignment) */
} highres_result_t;

/*
 * byte swabbing like libpcp/src/internal.h ... used for -Dfetch output only
 */
#ifdef HAVE_NETWORK_BYTEORDER
#define fetch_ntohpmID(a)	(a)
#define fetch_ntohll(a)		/* noop */
#else
#define fetch_ntohpmID(a)           htonl(a)
static void
fetch_htonll(char *p)
{
    char	c;
    int		i;

    for (i = 0; i < 4; i++) {
	c = p[i];
	p[i] = p[7-i];
	p[7-i] = c;
    }
}
#define fetch_ntohll(v) fetch_htonll(v)
#endif

int
myFetch(int numpmid, pmID pmidlist[], __pmResult **result)
{
    int			n = 0;
    int			fd; /* pmcd */
    int			sts;
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
		return myLocalFetch(ctxp, numpmid, pmidlist, result);
	    return PM_ERR_NOTHOST;
	}
    }
    else
	return PM_ERR_NOCONTEXT;

    if ((fd = ctxp->c_pmcd->pc_fd) < 0) {
	/* lost connection, try to get it back */
	n = reconnect();
	if (n < 0)
	    return n;
	fd = ctxp->c_pmcd->pc_fd;
    }

    if (ctxp->c_sent == 0) {
	/*
	 * current profile is _not_ already cached at other end of
	 * IPC, so send current profile
	 */
	if (pmDebugOptions.profile)
	    fprintf(stderr, "myFetch: calling __pmSendProfile, context: %d\n", ctx);
	if ((n = __pmSendProfile(fd, FROM_ANON, ctx, ctxp->c_instprof)) >= 0)
	    ctxp->c_sent = 1;
    }

    if (n >= 0) {
	pmID		*newlist = NULL;
	int		newcnt;
	int		have_dm;
	int		highres;
	int		pdutype;

	/* for derived metrics, may need to rewrite the pmidlist */
	have_dm = newcnt = __pmPrepareFetch(ctxp, numpmid, pmidlist, &newlist);
	if (newcnt > numpmid) {
	    /* replace args passed into myFetch */
	    numpmid = newcnt;
	    pmidlist = newlist;
	}

	if ((__pmFeaturesIPC(fd) & PDU_FLAG_HIGHRES)) {
	    pdutype = PDU_HIGHRES_FETCH;
	    highres = 1;
	} else {
	    pdutype = PDU_FETCH;
	    highres = 0;
	}
	n = __pmSendFetchPDU(fd, FROM_ANON, ctx, numpmid, pmidlist, pdutype);
	if (n >= 0) {
	    do {
		n = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb);
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
		    else if ((n == PDU_HIGHRES_RESULT && highres) ||
			     (n == PDU_RESULT && !highres)) {
			/*
			 * TODO: reassess this, is this still true? ...
			 *
			 * not safe to decode result here, so have to make
			 * do with a shallow dump of the PDU using logic
			 * from libpcp/__pmDecodeResult()
			 */
			int		lnumpmid;
			int		numval;
			int		i;
			int		vsize;
			pmID		pmid;
			char		*name;
			vlist_t		*vlp;
			__pmPDU		*data;

			/* assume PDU is valid ... it comes from pmcd */
			if (n == PDU_HIGHRES_RESULT) {
			    highres_result_t	*pp = (highres_result_t *)pb;
			    __pmTimestamp	timestamp;

			    data = &pp->data[0];
			    lnumpmid = ntohl(pp->numpmid);
			    memcpy(&timestamp, &pp->timestamp, sizeof(timestamp));
			    fetch_ntohll((char *)&timestamp.sec);
			    fetch_ntohll((char *)&timestamp.nsec);
			    fprintf(stderr, "pmHighResResult timestamp: %lld.%09d numpmid: %d\n", (long long)timestamp.sec, (int)timestamp.nsec, lnumpmid);
			} else {
			    result_t		*pp = (result_t *)pb;
			    struct timeval	timestamp;

			    data = &pp->data[0];
			    lnumpmid = ntohl(pp->numpmid);
			    timestamp.tv_sec = ntohl(pp->timestamp.tv_sec);
			    timestamp.tv_usec = ntohl(pp->timestamp.tv_usec);
			    fprintf(stderr, "pmResult timestamp: %d.%06d numpmid: %d\n", (int)timestamp.tv_sec, (int)timestamp.tv_usec, lnumpmid);
			}
			vsize = 0;
			for (i = 0; i < lnumpmid; i++) {
			    vlp = (vlist_t *)&data[vsize/sizeof(__pmPDU)];
			    pmid = fetch_ntohpmID(vlp->pmid);
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

		if ((n == PDU_HIGHRES_RESULT && highres) || (n == PDU_RESULT && !highres)) {
		    /* Success with a result in a PDU buffer */
		    PM_LOCK(ctxp->c_lock);
		    sts = (n == PDU_RESULT) ?
			    __pmDecodeResult_ctx(ctxp, pb, result) :
			    __pmDecodeHighResResult_ctx(ctxp, pb, result);
		    __pmUnpinPDUBuf(pb);
		    if (sts < 0)
			n = sts;
		    else if (have_dm)
			__pmFinishResult(ctxp, sts, result);
		    PM_UNLOCK(ctxp->c_lock);
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
		    n = PM_ERR_IPC;
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
		    __pmDumpPDUTrace(stderr);
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
