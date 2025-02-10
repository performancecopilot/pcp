/*
 * Copyright (c) 2013,2016-2018 Red Hat.
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
#include "libpcp.h"
#include "pmda.h"
#include "internal.h"
#include "fault.h"

static int
__pmRecvText(int fd, __pmContext *ctxp, int timeout, char **buffer)
{
    __pmPDU	*pb;
    int		sts, ident, pinpdu;

    pinpdu = sts = __pmGetPDU(fd, ANY_SIZE, timeout, &pb);
    if (sts == PDU_TEXT)
	sts = __pmDecodeText(pb, &ident, buffer);
    else if (sts == PDU_ERROR)
	__pmDecodeError(pb, &sts);
    else if (sts != PM_ERR_TIMEOUT) {
	if (pmDebugOptions.pdu) {
	    char	strbuf[20];
	    char	errmsg[PM_MAXERRMSGLEN];
	    if (sts < 0)
		fprintf(stderr, "__pmRecvText: PM_ERR_IPC: expecting PDU_TEXT but__pmGetPDU returns %d (%s)\n",
		    sts, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    else
		fprintf(stderr, "__pmRecvText: PM_ERR_IPC: expecting PDU_TEXT but__pmGetPDU returns %d (type=%s)\n",
		    sts, __pmPDUTypeStr_r(sts, strbuf, sizeof(strbuf)));
	}
	sts = PM_ERR_IPC;
    }

    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);

    return sts;
}

/*
 * Fallback to one-line, if possible, when no long-form text available
 */
static int
fallbacktext(int type, char *buffer)
{
    if ((type & PM_TEXT_DIRECT) != 0)
	return 0;
    if ((type & PM_TEXT_HELP) == 0)
	return 0;
    if (buffer == NULL || buffer[0] == '\0') {
	type &= ~PM_TEXT_HELP;
	type |= PM_TEXT_ONELINE;
	return type;
    }
    return 0;
}

static int
lookuptext(int ident, int type, char **buffer)
{
    __pmContext	*ctxp;
    __pmDSO	*dp;
    int		fd, ctx, sts, tout;

    if ((sts = ctx = pmWhichContext()) < 0)
	return sts;

    if ((ctxp = __pmHandleToPtr(ctx)) == NULL)
	return PM_ERR_NOCONTEXT;

    /* not set on all PDU error paths and fallbacktext() checks this ... */
    *buffer = NULL;

    if (ctxp->c_type == PM_CONTEXT_HOST) {
	tout = ctxp->c_pmcd->pc_tout_sec;
	fd = ctxp->c_pmcd->pc_fd;
again_host:
	sts = __pmSendTextReq(fd, __pmPtrToHandle(ctxp), ident, type);
	if (sts < 0)
	    sts = __pmMapErrno(sts);
	else {
	    PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_CALL);
	    sts = __pmRecvText(fd, ctxp, tout, buffer);
	    if (sts != 0 || *buffer[0] == '\0') {
		/* failed help text request, maybe try for one-line */
		if ((type = fallbacktext(type, *buffer)) != 0) {
		    if (*buffer != NULL) {
			free(*buffer);
			*buffer = NULL;
		    }
		    goto again_host;
		}
	    }
	}
    }
    else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
	    /* Local context requires single-threaded applications */
	    sts = PM_ERR_THREAD;
	else if ((dp = __pmLookupDSO(((__pmID_int *)&ident)->domain)) == NULL)
	    sts = PM_ERR_NOAGENT;
	else {
	    pmdaInterface *pmda = &dp->dispatch;
	    if (pmda->comm.pmda_interface >= PMDA_INTERFACE_5)
		    pmda->version.four.ext->e_context = ctx;
again_local:
	    sts = pmda->version.any.text(ident, type, buffer, pmda->version.any.ext);
	    if (sts != 0 || *buffer[0] == '\0') {
		/* failed help text request, maybe try for one-line */
		if ((type = fallbacktext(type, *buffer)) != 0)
		    goto again_local;
	    }
	    if (sts == 0)
	        /* Points into PMDAs local text - return a copy */
		*buffer = strdup(*buffer);
	}
    }
    else {
again_archive:
	sts = __pmLogLookupText(ctxp->c_archctl, ident, type, buffer);
	if (sts != 0 || *buffer[0] == '\0') {
	    /* failed help text request, maybe try for one-line */
	    if ((type = fallbacktext(type, *buffer)) != 0)
		goto again_archive;
	}
	if (sts == 0)
	    /* Points into archive hash tables - return a copy */
	    *buffer = strdup(*buffer);
    }

    PM_UNLOCK(ctxp->c_lock);
    return sts;
}

int
pmLookupText(pmID pmid, int level, char **buffer)
{
    /* need exactly one of PM_TEXT_ONELINE and PM_TEXT_HELP */
    if ((level & PM_TEXT_ONELINE) == PM_TEXT_ONELINE) {
	if ((level & PM_TEXT_HELP) == PM_TEXT_HELP)
	    return PM_ERR_ARG;
    }
    else {
	if ((level & PM_TEXT_HELP) == 0)
	    return PM_ERR_ARG;
    }

    if (IS_DERIVED(pmid)) {
	/*
	 * for derived metrics there is no indom help text, only metric
	 * help text, so don't need | PM_TEXT_PMID
	 */
	return __dmhelptext(pmid, level, buffer);
    }
    return lookuptext((int)pmid, level | PM_TEXT_PMID, buffer);
}

int
pmLookupInDomText(pmInDom indom, int level, char **buffer)
{
    return lookuptext((int)indom, level | PM_TEXT_INDOM, buffer);
}
