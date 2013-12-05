/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "pmda.h"
#include "internal.h"

int
pmLookupDesc(pmID pmid, pmDesc *desc)
{
    int		n;
    __pmContext	*ctxp;
    __pmPDU	*pb;

    if ((n = pmWhichContext()) < 0)
	goto done;
    if ((ctxp = __pmHandleToPtr(n)) == NULL) {
	n = PM_ERR_NOCONTEXT;
	goto done;
    }

    if (ctxp->c_type == PM_CONTEXT_HOST) {
	PM_LOCK(ctxp->c_pmcd->pc_lock);
	if ((n = __pmSendDescReq(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp), pmid)) < 0)
	    n = __pmMapErrno(n);
	else {
	    int		pinpdu;
	    pinpdu = n = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE,
				    ctxp->c_pmcd->pc_tout_sec, &pb);
	    if (n == PDU_DESC)
		n = __pmDecodeDesc(pb, desc);
	    else if (n == PDU_ERROR)
		__pmDecodeError(pb, &n);
	    else if (n != PM_ERR_TIMEOUT)
		n = PM_ERR_IPC;
	    if (pinpdu > 0)
		__pmUnpinPDUBuf(pb);
	}
	PM_UNLOCK(ctxp->c_pmcd->pc_lock);
    }
    else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	int		ctx = n;
	__pmDSO		*dp;
	if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
	    /* Local context requires single-threaded applications */
	    n = PM_ERR_THREAD;
	else if ((dp = __pmLookupDSO(((__pmID_int *)&pmid)->domain)) == NULL)
	    n = PM_ERR_NOAGENT;
	else {
	    if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		dp->dispatch.version.four.ext->e_context = ctx;
	    n = dp->dispatch.version.any.desc(pmid, desc, dp->dispatch.version.any.ext);
	}
    }
    else {
	/* assume PM_CONTEXT_ARCHIVE */
	n = __pmLogLookupDesc(ctxp->c_archctl->ac_log, pmid, desc);
    }

    if (n == PM_ERR_PMID || n == PM_ERR_PMID_LOG || n == PM_ERR_NOAGENT) {
	int		sts;
	/*
	 * check for derived metric ... keep error status from above
	 * unless we have success with the derived metrics
	 */
	sts = __dmdesc(ctxp, pmid, desc);
	if (sts >= 0)
	    n = sts;
    }
    PM_UNLOCK(ctxp->c_lock);

done:
    return n;
}

