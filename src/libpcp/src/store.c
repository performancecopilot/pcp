/*
 * Copyright (c) 2013 Red Hat.
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

/*
 * Internal variant of pmStore() ... ctxp is not NULL for
 * internal callers where the current context is already locked, but
 * NULL for callers from above the PMAPI or internal callers when the
 * current context is not locked.
 */
int
pmStore_ctx(__pmContext *ctxp, const pmResult *result)
{
    int		need_unlock = 0;
    int		n;
    int		sts;
    int		ctx;
    __pmDSO	*dp;

    if (result->numpmid < 1)
        return PM_ERR_TOOSMALL;

    for (n = 0; n < result->numpmid; n++) {
	if (result->vset[n]->numval < 1) {
	    return PM_ERR_VALUE;
	}
    }

    if ((ctx = pmWhichContext()) < 0) {
	return ctx;
    }

    if (ctxp == NULL) {
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	need_unlock = 1;
    }

    if (ctxp->c_type == PM_CONTEXT_HOST) {
	sts = __pmSendResult_ctx(ctxp, ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp), result);
	if (sts < 0)
	    sts = __pmMapErrno(sts);
	else {
	    __pmPDU	*pb;
	    int		pinpdu;
	 
PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_TIMEOUT);
	    pinpdu = sts = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE,
				      ctxp->c_pmcd->pc_tout_sec, &pb);
	    if (sts == PDU_ERROR)
		__pmDecodeError(pb, &sts);
	    else if (sts != PM_ERR_TIMEOUT)
		sts = PM_ERR_IPC;

	    if (pinpdu > 0)
		__pmUnpinPDUBuf(pb);
	}
    }
    else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	/*
	 * have to do them one at a time in case different DSOs
	 * involved ... need to copy each result->vset[n]
	 */
	pmResult	tmp;
	pmValueSet	tmpvset;

	if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA)) {
	    /* Local context requires single-threaded applications */
	    sts = PM_ERR_THREAD;
	} else {
	    sts = 0;
	    for (n = 0; sts == 0 && n < result->numpmid; n++) {
		if ((dp = __pmLookupDSO(((__pmID_int *)&result->vset[n]->pmid)->domain)) == NULL)
		    sts = PM_ERR_NOAGENT;
		else {
		    tmp.numpmid = 1;
		    tmp.vset[0] = &tmpvset;
		    tmpvset.numval = 1;
		    tmpvset.pmid = result->vset[n]->pmid;
		    tmpvset.valfmt = result->vset[n]->valfmt;
		    tmpvset.vlist[0] = result->vset[n]->vlist[0];
		    if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
			dp->dispatch.version.four.ext->e_context = ctx;
		    sts = dp->dispatch.version.any.store(&tmp, dp->dispatch.version.any.ext);
		}
	    }
	}
    }
    else {
	/* assume PM_CONTEXT_ARCHIVE -- this is an error */
	sts = PM_ERR_NOTHOST;
    }
    if (need_unlock)
	PM_UNLOCK(ctxp->c_lock);

    if (need_unlock) CHECK_C_LOCK;
    return sts;
}

int
pmStore(const pmResult *result)
{
    int	sts;
    sts = pmStore_ctx(NULL, result);
    CHECK_C_LOCK;
    return sts;
}
