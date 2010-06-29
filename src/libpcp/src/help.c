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
requesttext (__pmContext *ctxp, int ident, int type) 
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0) {
	return (PM_ERR_CTXBUSY);
    }

    n = __pmSendTextReq(ctxp->c_pmcd->pc_fd, PDU_BINARY, ident, type);
    if (n < 0) {
	n = __pmMapErrno(n);
    }

    return (n);
}

static int
receivetext (__pmContext *ctxp, char **buffer)
{
    int n;
    __pmPDU *pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY,
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_TEXT) {
	int x_ident;

	n = __pmDecodeText(pb, PDU_BINARY, &x_ident, buffer);
    }
    else if (n == PDU_ERROR)
	__pmDecodeError(pb, PDU_BINARY, &n);
    else if (n != PM_ERR_TIMEOUT)
	n = PM_ERR_IPC;

    return (n);
}

static int
lookuptext(int ident, int type, char **buffer)
{
    int		n;
    __pmContext	*ctxp;
    __pmDSO	*dp;


    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type == PM_CONTEXT_HOST) {
again:
	    if ((n = requesttext (ctxp, ident, type)) >= 0) {
		n = receivetext (ctxp, buffer);

		/*
		 * Note: __pmDecodeText does not swab ident because it
		 * does not know whether it's a pmID or a pmInDom.
		 */

		if (n == 0 && (*buffer)[0] == '\0' && (type & PM_TEXT_HELP)) {
		    /* fall back to oneline, if possible */
		    free(*buffer);
		    type &= ~PM_TEXT_HELP;
		    type |= PM_TEXT_ONELINE;
		    goto again;
		}
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    if ((dp = __pmLookupDSO(((__pmID_int *)&ident)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
again_local:
		if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_4)
		    n = dp->dispatch.version.four.text(ident, type, buffer, dp->dispatch.version.four.ext);
		else if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_3 ||
		         dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_2)
		    n = dp->dispatch.version.two.text(ident, type, buffer, dp->dispatch.version.two.ext);
		else
		    n = dp->dispatch.version.one.text(ident, type, buffer);
		if (n == 0 && (*buffer)[0] == '\0' && (type & PM_TEXT_HELP)) {
		    /* fall back to oneline, if possible */
		    type &= ~PM_TEXT_HELP;
		    type |= PM_TEXT_ONELINE;
		    goto again_local;
		}
		if (n == 0 && dp->dispatch.comm.pmda_interface != PMDA_INTERFACE_1) {
		    /*
		     * PMDAs after PMDA_INTERFACE_1 don't malloc the buffer
		     * but the caller will free it, so malloc and copy
		     */
		    *buffer = strdup(*buffer);
		} else if (n < 0 &&
		    dp->dispatch.comm.pmapi_version == PMAPI_VERSION_1) {
			n = XLATE_ERR_1TO2(n);
		}
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE -- this is an error */
	    return PM_ERR_NOTHOST;
	}
    }

    return n;
}

static int
ctxidRequestText (int ctx, int id, int level)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetHostContextByID(ctx, &ctxp)) >= 0) {
	if ((n = requesttext (ctxp, id, level)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_TEXT_REQ;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }
    return (n);
}

int
pmReceiveText (int ctx, char **buffer)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetBusyHostContextByID(ctx, &ctxp, PDU_TEXT_REQ)) >= 0) {
	n = receivetext (ctxp, buffer);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }
    return (n);
}

int
pmRequestText (int ctx, pmID pmid, int level)
{
    return (ctxidRequestText (ctx, (int)pmid, level | PM_TEXT_PMID));
}

int
pmRequestInDomText (int ctx, pmID pmid, int level)
{
    return (ctxidRequestText (ctx, (int)pmid, level | PM_TEXT_INDOM));
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
