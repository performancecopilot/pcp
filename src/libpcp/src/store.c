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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

static int
sendstore (__pmContext *ctxp, const pmResult *result)
{
    int sts;

#ifdef ASYNC_API
    if (ctxp->c_pmcd->pc_curpdu != 0) {
	return (PM_ERR_CTXBUSY);
    }
#endif /*ASYNC_API*/

    sts = __pmSendResult(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp), result);
    if (sts < 0) {
	sts = __pmMapErrno(sts);
    }

    return (sts);
}

#ifdef ASYNC_API
int
pmStoreSend (int ctx, const pmResult *result)
{
    int sts;
    __pmContext *ctxp;

    if ((sts = __pmGetHostContextByID(ctx, &ctxp)) >= 0) {
	if ((sts = sendstore (ctxp, result)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_RESULT;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }

    return (sts);
}
#endif /*ASYNC_API*/

static int
store_check (__pmContext *ctxp)
{
    int sts;
    __pmPDU	*pb;
 
    sts = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE,
		     ctxp->c_pmcd->pc_tout_sec, &pb);
    if (sts == PDU_ERROR)
	__pmDecodeError(pb, &sts);
    else if (sts != PM_ERR_TIMEOUT)
	sts = PM_ERR_IPC;

    return (sts);
}

#ifdef ASYNC_API
int 
pmStoreCheck (int ctx)
{
    int sts;
    __pmContext	*ctxp;

    if ((sts = __pmGetHostContextByID(ctx, &ctxp)) >= 0) {
	if (ctxp->c_pmcd->pc_curpdu != PDU_RESULT) {
	    return (PM_ERR_CTXBUSY);
	}

	sts = store_check (ctxp);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }

    return (sts);
}
#endif /*ASYNC_API*/

int
pmStore(const pmResult *result)
{
    int		n;
    int		sts;
    __pmContext	*ctxp;
    __pmDSO	*dp;

    if (result->numpmid < 1)
        return PM_ERR_TOOSMALL;

    for (n = 0; n < result->numpmid; n++) {
	if (result->vset[n]->numval < 1) {
	    return PM_ERR_VALUE;
	}
    }

    if ((sts = pmWhichContext()) >= 0) {
	int	ctx = n;
	ctxp = __pmHandleToPtr(sts);
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if ((sts = sendstore (ctxp, result)) >= 0) {
		sts = store_check (ctxp);
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    /*
	     * have to do them one at a time in case different DSOs
	     * involved ... need to copy each result->vset[n]
	     */
	    pmResult	tmp;
	    pmValueSet	tmpvset;
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
		    if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_4)
			sts = dp->dispatch.version.four.store(&tmp,
						dp->dispatch.version.four.ext);
		    else
			sts = dp->dispatch.version.two.store(&tmp,
						dp->dispatch.version.two.ext);
		}
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE -- this is an error */
	    return PM_ERR_NOTHOST;
	}
    }

    return sts;
}
