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

static int
lookuptext(int ident, int type, char **buffer)
{
    int		n;
    __pmContext	*ctxp;
    __pmDSO	*dp;


    if ((n = pmWhichContext()) >= 0) {
	int	ctx = n;
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    PM_LOCK(ctxp->c_pmcd->pc_lock);
again:
	    n = __pmSendTextReq(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp), ident, type);
	    if (n < 0)
		n = __pmMapErrno(n);
	    else {
		__pmPDU	*pb;
		int		pinpdu;
		pinpdu = n = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE,
					ctxp->c_pmcd->pc_tout_sec, &pb);
		if (n == PDU_TEXT) {
		    int x_ident;
		    n = __pmDecodeText(pb, &x_ident, buffer);
		}
		else if (n == PDU_ERROR)
		    __pmDecodeError(pb, &n);
		else if (n != PM_ERR_TIMEOUT)
		    n = PM_ERR_IPC;
		if (pinpdu > 0)
		    __pmUnpinPDUBuf(pb);
		/*
		 * Note: __pmDecodeText does not swab ident because it
		 * does not know whether it's a pmID or a pmInDom.
		 */

		if (n == 0 && (*buffer)[0] == '\0' && (type & PM_TEXT_HELP)) {
		    /* fall back to one-line, if possible */
		    free(*buffer);
		    type &= ~PM_TEXT_HELP;
		    type |= PM_TEXT_ONELINE;
		    goto again;
		}
	    }
	    PM_UNLOCK(ctxp->c_pmcd->pc_lock);
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
		/* Local context requires single-threaded applications */
		n = PM_ERR_THREAD;
	    else if ((dp = __pmLookupDSO(((__pmID_int *)&ident)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
again_local:
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dp->dispatch.version.four.ext->e_context = ctx;
		n = dp->dispatch.version.any.text(ident, type, buffer, dp->dispatch.version.any.ext);
		if (n == 0 && (*buffer)[0] == '\0' && (type & PM_TEXT_HELP)) {
		    /* fall back to one-line, if possible */
		    type &= ~PM_TEXT_HELP;
		    type |= PM_TEXT_ONELINE;
		    goto again_local;
		}
		if (n == 0) {
		    /*
		     * PMDAs don't malloc the buffer but the caller will
		     * free it, so malloc and copy
		     */
		    *buffer = strdup(*buffer);
		}
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE -- this is an error */
	    n = PM_ERR_NOTHOST;
	}
	PM_UNLOCK(ctxp->c_lock);
    }

    return n;
}

int
pmLookupText(pmID pmid, int level, char **buffer)
{
    return lookuptext((int)pmid, level | PM_TEXT_PMID, buffer);
}

int
pmLookupInDomText(pmInDom indom, int level, char **buffer)
{
    return lookuptext((int)indom, level | PM_TEXT_INDOM, buffer);
}
