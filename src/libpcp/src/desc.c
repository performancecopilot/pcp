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
#include "fault.h"

static int
__pmRecvDesc(int fd, __pmContext *ctxp, int timeout, pmDesc *desc)
{
    int		sts, pinpdu;
    __pmPDU	*pb;

    pinpdu = sts = __pmGetPDU(fd, ANY_SIZE, timeout, &pb);
    if (sts == PDU_DESC)
	sts = __pmDecodeDesc(pb, desc);
    else if (sts == PDU_ERROR)
	__pmDecodeError(pb, &sts);
    else if (sts != PM_ERR_TIMEOUT)
	sts = PM_ERR_IPC;

    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);

    return sts;
}

/*
 * Internal variant of pmLookupDesc() ... ctxp is not NULL for
 * internal callers where the current context is already locked, but
 * NULL for callers from above the PMAPI or internal callers when the
 * current context is not locked.
 */
int
pmLookupDesc_ctx(__pmContext *ctxp, pmID pmid, pmDesc *desc)
{
    int		need_unlock = 0;
    __pmDSO	*dp;
    int		fd, ctx, sts, tout;

    if ((sts = ctx = pmWhichContext()) < 0)
	return sts;

    if (ctxp == NULL) {
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	need_unlock = 1;
    }

    if (ctxp->c_type == PM_CONTEXT_HOST) {
	tout = ctxp->c_pmcd->pc_tout_sec;
	fd = ctxp->c_pmcd->pc_fd;
	if ((sts = __pmSendDescReq(fd, __pmPtrToHandle(ctxp), pmid)) < 0) {
	    sts = __pmMapErrno(sts);
	} else {
	    PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_TIMEOUT);
	    sts = __pmRecvDesc(fd, ctxp, tout, desc);
	}
    }
    else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
	    /* Local context requires single-threaded applications */
	    sts = PM_ERR_THREAD;
	else if ((dp = __pmLookupDSO(((__pmID_int *)&pmid)->domain)) == NULL)
	    sts = PM_ERR_NOAGENT;
	else {
	    pmdaInterface *pmda = &dp->dispatch;
	    if (pmda->comm.pmda_interface >= PMDA_INTERFACE_5)
		pmda->version.four.ext->e_context = ctx;
	    sts = pmda->version.any.desc(pmid, desc, pmda->version.any.ext);
	}
    }
    else {
	/* assume PM_CONTEXT_ARCHIVE */
	sts = __pmLogLookupDesc(ctxp->c_archctl->ac_log, pmid, desc);
    }

    if (sts == PM_ERR_PMID || sts == PM_ERR_PMID_LOG || sts == PM_ERR_NOAGENT) {
	int		sts2;
	/*
	 * check for derived metric ... keep error status from above
	 * unless we have success with the derived metrics
	 */
	sts2 = __dmdesc(ctxp, pmid, desc);
	if (sts2 >= 0)
	    sts = sts2;
    }
    if (need_unlock)
	PM_UNLOCK(ctxp->c_lock);

    if (need_unlock) CHECK_C_LOCK;
    return sts;
}

int
pmLookupDesc(pmID pmid, pmDesc *desc)
{
    int	sts;
    sts = pmLookupDesc_ctx(NULL, pmid, desc);
    CHECK_C_LOCK;
    return sts;
}

