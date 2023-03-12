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
		 * expect PDU_[HIGHRES_]RESULT or
		 *        PDU_ERROR(changed > 0)+PDU_[HIGHRES_]RESULT or
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
			if (sts & PMCD_HOSTNAME_CHANGE) {
			    if (flag++)
				fprintf(stderr, ", ");
			    fprintf(stderr, "hostname change");
			}
			fputc('\n', stderr);
		    }
		    else if (n == PDU_HIGHRES_RESULT && !highres)
			fprintf(stderr, "__pmGetPDU: bad PDU_HIGHRES_RESULT\n");
		    else if (n == PDU_RESULT && highres)
			fprintf(stderr, "__pmGetPDU: bad PDU_RESULT\n");
		    else
			fprintf(stderr, "__pmGetPDU: Error: %s\n", pmErrStr(n));
		}

		if ((n == PDU_HIGHRES_RESULT && highres) ||
		    (n == PDU_RESULT && !highres)) {
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

	    if (changed & PMCD_HOSTNAME_CHANGE) {
		/*
		 * Hostname changed for pmcd and we were launched from
		 * the control-driven scripts (pmlogger_check, pmlogger_daily)
		 * or re-exec'd, then we need to exit.
		 *
		 * We rely on the systemd autorestart, systemd timer,
		 * cron or the user to restart this pmlogger at which
		 * time one or more of the following will happen:
		 * - the correct pmcd hostname will appear in the archive
		 *   label record
		 * - for a pmlogger launched from the standard
		 *   /etc/pcp/pmlogger control files, LOCALHOSTNAME will get
		 *   correctly re-translated into a different pathname
		 *   (usually the directory for the archive)
		 */
		if (runfromcontrol) {
		    run_done(0, "PMCD hostname changed");
		    /* NOTREACHED */
		}
		pmNotifyErr(LOG_INFO, "PMCD hostname changed");
	    }
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
