/*
 * Copyright (c) 2021 Red Hat.
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
__pmRecvDesc(int fd, __pmContext *ctxp, int timeout, pmDesc *desc)
{
    int		sts, pinpdu;
    __pmPDU	*pb;

    pinpdu = sts = __pmGetPDU(fd, ANY_SIZE, timeout, &pb);
    if (sts == PDU_DESC)
	sts = __pmDecodeDesc(pb, desc);
    else if (sts == PDU_ERROR)
	__pmDecodeError(pb, &sts);
    else if (sts != PM_ERR_TIMEOUT) {
	if (pmDebugOptions.pdu) {
	    char	strbuf[20];
	    char	errmsg[PM_MAXERRMSGLEN];
	    if (sts < 0)
		fprintf(stderr, "__pmRecvDesc: PM_ERR_IPC: expecting PDU_DESC but__pmGetPDU returns %d (%s)\n",
		    sts, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    else
		fprintf(stderr, "__pmRecvDesc: PM_ERR_IPC: expecting PDU_DESC but__pmGetPDU returns %d (type=%s)\n",
		    sts, __pmPDUTypeStr_r(sts, strbuf, sizeof(strbuf)));
	}
	sts = PM_ERR_IPC;
    }

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
pmLookupDesc_ctx(__pmContext *ctxp, int derive_locked, pmID pmid, pmDesc *desc)
{
    int		need_unlock = 0;
    __pmDSO	*dp;
    int		fd, ctx, sts, tout;

    if (pmDebugOptions.pmapi) {
	char    dbgbuf[20];
	fprintf(stderr, "pmLookupDesc(%s, ...) <:", pmIDStr_r(pmid, dbgbuf, sizeof(dbgbuf)));
    }

    if ((sts = ctx = pmWhichContext()) < 0)
	goto pmapi_return;

    if (ctxp == NULL) {
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp == NULL) {
	    sts = PM_ERR_NOCONTEXT;
	    goto pmapi_return;
	}
	need_unlock = 1;
    }
    else
	PM_ASSERT_IS_LOCKED(ctxp->c_lock);

    if (ctxp->c_type == PM_CONTEXT_HOST) {
	tout = ctxp->c_pmcd->pc_tout_sec;
	fd = ctxp->c_pmcd->pc_fd;
	if ((sts = __pmSendDescReq(fd, __pmPtrToHandle(ctxp), pmid)) < 0) {
	    sts = __pmMapErrno(sts);
	} else {
	    PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_CALL);
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
	sts = __pmLogLookupDesc(ctxp->c_archctl, pmid, desc);
    }

    if (sts == PM_ERR_PMID || sts == PM_ERR_PMID_LOG || sts == PM_ERR_NOAGENT) {
	int		sts2;
	/*
	 * check for derived metric ... keep error status from above
	 * unless we have success with the derived metrics, except that
	 * PM_ERR_BADDERIVE really means the derived metric bind failed,
	 * so we should propagate that one back ...
	 */
	sts2 = __dmdesc(ctxp, derive_locked, pmid, desc);
	if (sts2 >= 0 || sts2 == PM_ERR_BADDERIVE)
	    sts = sts2;
    }
    if (need_unlock)
	PM_UNLOCK(ctxp->c_lock);

pmapi_return:

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, ":> returns ");
	if (sts >= 0)
	    fprintf(stderr, "%d\n", sts);
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }

    return sts;
}

int
pmLookupDesc(pmID pmid, pmDesc *desc)
{
    int	sts;
    sts = pmLookupDesc_ctx(NULL, PM_NOT_LOCKED, pmid, desc);
    return sts;
}

static int
__pmRecvDescs(int fd, __pmContext *ctxp, int timeout, int numdescs, pmDesc *desclist)
{
    int		sts, pinpdu;
    __pmPDU	*pb;

    pinpdu = sts = __pmGetPDU(fd, ANY_SIZE, timeout, &pb);
    if (sts == PDU_DESCS)
	sts = __pmDecodeDescs(pb, numdescs, desclist);
    else if (sts == PDU_ERROR)
	__pmDecodeError(pb, &sts);
    else if (sts != PM_ERR_TIMEOUT) {
	if (pmDebugOptions.pdu) {
	    char	strbuf[20];
	    char	errmsg[PM_MAXERRMSGLEN];
	    if (sts < 0)
		fprintf(stderr, "__pmRecvDescs: PM_ERR_IPC: expecting PDU_DESCS but__pmGetPDU returns %d (%s)\n",
		    sts, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    else
		fprintf(stderr, "__pmRecvDescs: PM_ERR_IPC: expecting PDU_DESCS but__pmGetPDU returns %d (type=%s)\n",
		    sts, __pmPDUTypeStr_r(sts, strbuf, sizeof(strbuf)));
	}
	sts = PM_ERR_IPC;
    }

    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);

    return sts;
}

/*
 * Internal variant of pmLookupDescs() ... ctxp is not NULL for
 * internal callers where the current context is already locked, but
 * NULL for callers from above the PMAPI or internal callers when the
 * current context is not locked.
 */
int
pmLookupDescs_ctx(__pmContext *ctxp, int derive_locked, int numpmid, pmID *pmidlist, pmDesc *desclist)
{
    __pmDSO	*dp;
    pmID	pmid;
    int		nfail = 0;
    int		need_unlock = 0;
    int		i, fd, ctx, sts, lsts, tout;

    if (pmDebugOptions.pmapi) {
	char	strbuf[20];
	pmIDStr_r(pmidlist[0], strbuf, sizeof(strbuf));
	fprintf(stderr, "pmLookupDescs(%d, desc[0] %s", numpmid, strbuf);
	if (numpmid > 1) {
	    pmIDStr_r(pmidlist[numpmid-1], strbuf, sizeof(strbuf));
	    fprintf(stderr, " ... desc[%d] %s", numpmid-1, strbuf);
	}
	fprintf(stderr, ", ...) <:");
    }

    if (numpmid < 1) {
	if (pmDebugOptions.pmapi) {
	    fprintf(stderr, "pmLookupDescs(%d, ...) bad numpmid!\n", numpmid);
	}
	sts = PM_ERR_TOOSMALL;
	goto pmapi_return;
    }

    if ((sts = ctx = pmWhichContext()) < 0)
	goto pmapi_return;

    if (ctxp == NULL) {
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp == NULL) {
	    sts = PM_ERR_NOCONTEXT;
	    goto pmapi_return;
	}
	need_unlock = 1;
    }
    else
	PM_ASSERT_IS_LOCKED(ctxp->c_lock);

    if (ctxp->c_type == PM_CONTEXT_HOST) {
	tout = ctxp->c_pmcd->pc_tout_sec;
	fd = ctxp->c_pmcd->pc_fd;

	if ((__pmFeaturesIPC(fd) & PDU_FLAG_DESCS)) {
	    /* Use the bulk-transfer mechanism from a more modern pmcd */
	    ctx = __pmPtrToHandle(ctxp);
	    for (i = sts = 0; i < numpmid; i++)
		if (!IS_DERIVED(pmidlist[i]))
		    break;
	    if (i == numpmid)	/* special case :- all derived metrics */
		nfail++;
	    else if ((sts = __pmSendIDList(fd, ctx, numpmid, pmidlist, -1)) < 0)
		sts = __pmMapErrno(sts);
	    else {
		PM_FAULT_POINT("libpcp/" __FILE__ ":2", PM_FAULT_CALL);
		sts = __pmRecvDescs(fd, ctxp, tout, numpmid, desclist);
		nfail = (sts >= 0) ? numpmid - sts : numpmid;
	    }
	} else {
	    /* Fallback for down-revision pmcd, desc lookups in a loop */
	    for (i = sts = 0; i < numpmid; i++) {
		pmid = pmidlist[i];
		desclist[i].pmid = PM_ID_NULL;
		if (IS_DERIVED(pmid)) {
		    lsts = PM_ERR_GENERIC;
		    nfail++;
		} else if ((lsts = __pmSendDescReq(fd, ctx, pmid)) < 0) {
		    lsts = __pmMapErrno(lsts);
		} else {
		    PM_FAULT_POINT("libpcp/" __FILE__ ":3", PM_FAULT_CALL);
		    lsts = __pmRecvDesc(fd, ctxp, tout, &desclist[i]);
		}
		if (lsts >= 0)
		    sts++;
	    }
	}
    }
    else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
	    /* Local context requires single-threaded applications */
	    sts = PM_ERR_THREAD;
	else {
	    for (i = sts = 0; i < numpmid; i++) {
		pmid = pmidlist[i];
		desclist[i].pmid = PM_ID_NULL;
		if (IS_DERIVED(pmid)) {
		    lsts = PM_ERR_GENERIC;
		    nfail++;
		}
		else if ((dp = __pmLookupDSO(((__pmID_int *)&pmid)->domain)) == NULL) {
		    lsts = PM_ERR_NOAGENT;
		} else {
		    pmdaInterface *pmda = &dp->dispatch;
		    if (pmda->comm.pmda_interface >= PMDA_INTERFACE_5)
			pmda->version.four.ext->e_context = ctx;
		    lsts = pmda->version.any.desc(pmid, &desclist[i], pmda->version.any.ext);
		}
		if (lsts >= 0)
		    sts++;
	    }
	}
    }
    else {
	for (i = sts = 0; i < numpmid; i++) {
	    pmid = pmidlist[i];
	    desclist[i].pmid = PM_ID_NULL;
	    if (IS_DERIVED(pmid)) {
		lsts = PM_ERR_GENERIC;
		nfail++;
	    } else {
		lsts = __pmLogLookupDesc(ctxp->c_archctl, pmid, &desclist[i]);
	    }
	    if (lsts >= 0)
		sts++;
	}
    }

    if (nfail > 0) {
	for (i = 0; i < numpmid; i++) {
	    pmid = pmidlist[i];
	    if (!IS_DERIVED(pmid))
		continue;
	    /*
	     * check for derived metric ... keep error status from above
	     * unless we have success with the derived metrics, except that
	     * PM_ERR_BADDERIVE really means the derived metric bind failed,
	     * so we should propagate that one back if possible ...
	     */
	    lsts = __dmdesc(ctxp, derive_locked, pmid, &desclist[i]);
	    if (lsts >= 0 && sts >= 0)
		sts++;
	    else if (lsts == PM_ERR_BADDERIVE && numpmid == 1)
		sts = lsts;
	}
    }

    if (need_unlock)
	PM_UNLOCK(ctxp->c_lock);

pmapi_return:

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, ":> returns ");
	if (sts >= 0)
	    fprintf(stderr, "%d\n", sts);
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }

    return sts;
}

int
pmLookupDescs(int numpmid, pmID *pmidlist, pmDesc *desclist)
{
    int	sts;
    sts = pmLookupDescs_ctx(NULL, PM_NOT_LOCKED, numpmid, pmidlist, desclist);
    return sts;
}
