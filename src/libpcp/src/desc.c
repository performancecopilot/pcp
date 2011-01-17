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
request_desc (__pmContext *ctxp, pmID pmid)
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0) {
	return (PM_ERR_CTXBUSY);
    }

    if ((n = __pmSendDescReq(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp), pmid)) < 0) {
	n = __pmMapErrno(n);
    }

    return (n);
}

int
pmRequestDesc (int ctx, pmID pmid)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetHostContextByID(ctx, &ctxp)) >= 0) {
	if ((n = request_desc (ctxp, pmid)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_DESC_REQ;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }
    return (n);
}

static int
receive_desc (__pmContext *ctxp, pmDesc *desc)
{
    int n;
    __pmPDU	*pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE,
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_DESC)
	n = __pmDecodeDesc(pb, desc);
    else if (n == PDU_ERROR)
	__pmDecodeError(pb, &n);
    else if (n != PM_ERR_TIMEOUT)
	n = PM_ERR_IPC;

    return (n);
}

int
pmReceiveDesc(int ctx, pmDesc *desc)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetBusyHostContextByID(ctx, &ctxp, PDU_DESC_REQ)) >= 0) {
	n = receive_desc (ctxp, desc);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }

    return (n);
}

int
pmLookupDesc(pmID pmid, pmDesc *desc)
{
    int		n;
    __pmContext	*ctxp;
    __pmDSO	*dp;

    if ((n = pmWhichContext()) >= 0) {
	int	ctx = n;
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if ((n = request_desc(ctxp, pmid)) >= 0) {
		n = receive_desc(ctxp, desc);
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    if ((dp = __pmLookupDSO(((__pmID_int *)&pmid)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dp->dispatch.version.four.ext->e_context = ctx;
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_4)
		    n = dp->dispatch.version.four.desc(pmid, desc, dp->dispatch.version.four.ext);
		else if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_3 ||
		         dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_2)
		    n = dp->dispatch.version.two.desc(pmid, desc, dp->dispatch.version.two.ext);
		else
		    n = dp->dispatch.version.one.desc(pmid, desc);
		if (n < 0 &&
		    dp->dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			n = XLATE_ERR_1TO2(n);
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
    }

    return n;
}

