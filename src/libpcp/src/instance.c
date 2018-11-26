/*
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 1995-2006 Silicon Graphics, Inc.  All Rights Reserved.
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

int
pmLookupInDom_ctx(__pmContext *ctxp, pmInDom indom, const char *name)
{
    int		sts;
    pmInResult	*result;
    int		need_unlock = 0;

    if (pmDebugOptions.pmapi) {
	char    dbgbuf[20];
	fprintf(stderr, "pmLookupInDom(%s, %s) <:", pmInDomStr_r(indom, dbgbuf, sizeof(dbgbuf)), name);
    }

    if (indom == PM_INDOM_NULL) {
	sts = PM_ERR_INDOM;
	goto pmapi_return;
    }

    if ((sts = pmWhichContext()) >= 0) {
	int	ctx = sts;
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
	    sts = __pmSendInstanceReq(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp),
				    &ctxp->c_origin, indom, PM_IN_NULL, name);
	    if (sts < 0)
		sts = __pmMapErrno(sts);
	    else {
		__pmPDU	*pb;
		int	pinpdu;

PM_FAULT_POINT("libpcp/" __FILE__ ":3", PM_FAULT_TIMEOUT);
		pinpdu = sts = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE, 
			       ctxp->c_pmcd->pc_tout_sec, &pb);
		if (sts == PDU_INSTANCE) {
		    pmInResult	*result;
		    if ((sts = __pmDecodeInstance(pb, &result)) >= 0) {
			sts = result->instlist[0];
			__pmFreeInResult(result);
		    }
		}
		else if (sts == PDU_ERROR)
		    __pmDecodeError(pb, &sts);
		else if (sts != PM_ERR_TIMEOUT)
		    sts = PM_ERR_IPC;

		if (pinpdu > 0)
		    __pmUnpinPDUBuf(pb);
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    __pmDSO		*dp;
	    if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
		/* Local context requires single-threaded applications */
		sts = PM_ERR_THREAD;
	    else if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		sts = PM_ERR_NOAGENT;
	    else {
		/* We can safely cast away const here */
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dp->dispatch.version.four.ext->e_context = ctx;
		sts = dp->dispatch.version.any.instance(indom, PM_IN_NULL, 
							(char *)name, &result, 
							dp->dispatch.version.any.ext);
	    }
	    if (sts >= 0) {
		sts = result->instlist[0];
		__pmFreeInResult(result);
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    sts = __pmLogLookupInDom(ctxp->c_archctl, indom, &ctxp->c_origin, name);
	}
	if (need_unlock)
	    PM_UNLOCK(ctxp->c_lock);
    }

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
pmLookupInDom(pmInDom indom, const char *name)
{
    int	sts;
    sts = pmLookupInDom_ctx(NULL, indom, name);
    return sts;
}

/*
 * Internal variant of pmNameInDom() ... ctxp is not NULL for
 * internal callers where the current context is already locked, but
 * NULL for callers from above the PMAPI or internal callers when the
 * current context is not locked.
 */
int
pmNameInDom_ctx(__pmContext *ctxp, pmInDom indom, int inst, char **name)
{
    int		need_unlock = 0;
    int		sts;
    pmInResult	*result;

    if (pmDebugOptions.pmapi) {
	char    dbgbuf[20];
	fprintf(stderr, "pmNameInDom(%s, %d, ...) <:", pmInDomStr_r(indom, dbgbuf, sizeof(dbgbuf)), inst);
    }

    if (indom == PM_INDOM_NULL) {
	sts = PM_ERR_INDOM;
	goto pmapi_return;
    }

    if ((sts = pmWhichContext()) >= 0) {
	int	ctx = sts;
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
	    sts = __pmSendInstanceReq(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp),
				    &ctxp->c_origin, indom, inst, NULL);
	    if (sts < 0)
		sts = __pmMapErrno(sts);
	    else {
		__pmPDU	*pb;
		int	pinpdu;

PM_FAULT_POINT("libpcp/" __FILE__ ":2", PM_FAULT_TIMEOUT);
		pinpdu = sts = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE,
					ctxp->c_pmcd->pc_tout_sec, &pb);
		if (sts == PDU_INSTANCE) {
		    pmInResult *result;
		    if ((sts = __pmDecodeInstance(pb, &result)) >= 0) {
			if ((*name = strdup(result->namelist[0])) == NULL)
			    sts = -oserror();
			__pmFreeInResult(result);
		    }
		}
		else if (sts == PDU_ERROR)
		    __pmDecodeError(pb, &sts);
		else if (sts != PM_ERR_TIMEOUT)
		    sts = PM_ERR_IPC;

		if (pinpdu > 0)
		    __pmUnpinPDUBuf(pb);
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    __pmDSO	*dp;
	    if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
		/* Local context requires single-threaded applications */
		sts = PM_ERR_THREAD;
	    else if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		sts = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dp->dispatch.version.four.ext->e_context = ctx;
		sts = dp->dispatch.version.any.instance(indom, inst, NULL, &result, dp->dispatch.version.any.ext);
	    }
	    if (sts >= 0) {
		if ((*name = strdup(result->namelist[0])) == NULL)
		    sts = -oserror();
		__pmFreeInResult(result);
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    char	*tmp;
	    if ((sts = __pmLogNameInDom(ctxp->c_archctl, indom, &ctxp->c_origin, inst, &tmp)) >= 0) {
		if ((*name = strdup(tmp)) == NULL)
		    sts = -oserror();
	    }
	}
	if (need_unlock)
	    PM_UNLOCK(ctxp->c_lock);
    }

pmapi_return:

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, ":> returns ");
	if (sts >= 0)
	    fprintf(stderr, "%d (name %s)\n", sts, *name);
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }

    return sts;
}

int
pmNameInDom(pmInDom indom, int inst, char **name)
{
    int	sts;
    sts = pmNameInDom_ctx(NULL, indom, inst, name);
    return sts;
}

static int
inresult_to_lists(pmInResult *result, int **instlist, char ***namelist)
{
    int n, i, sts, need;
    char *p;
    int *ilist;
    char **nlist;
    
    if (result->numinst == 0) {
	__pmFreeInResult(result);
	return 0;
    }
    need = 0;
    for (i = 0; i < result->numinst; i++) {
	need += sizeof(**namelist) + strlen(result->namelist[i]) + 1;
    }
    ilist = (int *)malloc(result->numinst * sizeof(result->instlist[0]));
    if (ilist == NULL) {
	sts = -oserror();
	__pmFreeInResult(result);
	return sts;
    }
    if ((nlist = (char **)malloc(need)) == NULL) {
	sts = -oserror();
	free(ilist);
	__pmFreeInResult(result);
	return sts;
    }

    *instlist = ilist;
    *namelist = nlist;
    p = (char *)&nlist[result->numinst];
    for (i = 0; i < result->numinst; i++) {
	ilist[i] = result->instlist[i];
	strcpy(p, result->namelist[i]);
	nlist[i] = p;
	p += strlen(result->namelist[i]) + 1;
    }
    n = result->numinst;
    __pmFreeInResult(result);
    return n;
}

int
pmGetInDom(pmInDom indom, int **instlist, char ***namelist)
{
    int			sts;
    int			i;
    pmInResult	*result;
    __pmContext		*ctxp;
    char		*p;
    int			need;
    int			*ilist = NULL;
    char		**nlist = NULL;

    if (pmDebugOptions.pmapi) {
	char    dbgbuf[20];
	fprintf(stderr, "pmGetInDom(%s, ...) <:", pmInDomStr_r(indom, dbgbuf, sizeof(dbgbuf)));
    }

    if (indom == PM_INDOM_NULL) {
	sts = PM_ERR_INDOM;
	goto pmapi_return;
    }

    if ((sts = pmWhichContext()) >= 0) {
	int	ctx = sts;
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp == NULL) {
	    sts = PM_ERR_NOCONTEXT;
	    goto pmapi_return;
	}
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    sts = __pmSendInstanceReq(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp),
				    &ctxp->c_origin, indom, PM_IN_NULL, NULL);
	    if (sts < 0)
		sts = __pmMapErrno(sts);
	    else {
		__pmPDU	*pb;
		int	pinpdu;

PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_TIMEOUT);
		pinpdu = sts = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE,
					ctxp->c_pmcd->pc_tout_sec, &pb);
		if (sts == PDU_INSTANCE) {
		    pmInResult	*result;
		    if ((sts = __pmDecodeInstance(pb, &result)) < 0) {
			if (pinpdu > 0)
			    __pmUnpinPDUBuf(pb);
			PM_UNLOCK(ctxp->c_lock);
			goto pmapi_return;
		    }
		    sts = inresult_to_lists(result, instlist, namelist);
		}
		else if (sts == PDU_ERROR)
		    __pmDecodeError(pb, &sts);
		else if (sts != PM_ERR_TIMEOUT)
		    sts = PM_ERR_IPC;

		if (pinpdu > 0)
		    __pmUnpinPDUBuf(pb);
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    __pmDSO	*dp;
	    if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
		/* Local context requires single-threaded applications */
		sts = PM_ERR_THREAD;
	    else if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		sts = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dp->dispatch.version.four.ext->e_context = ctx;
		sts = dp->dispatch.version.any.instance(indom, PM_IN_NULL, NULL,
					       &result,
					       dp->dispatch.version.any.ext);
	    }
	    if (sts >= 0)
		sts = inresult_to_lists(result, instlist, namelist);
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    int		*insttmp;
	    char	**nametmp;
	    if ((sts = __pmLogGetInDom(ctxp->c_archctl, indom, &ctxp->c_origin, &insttmp, &nametmp)) >= 0) {
		need = 0;
		for (i = 0; i < sts; i++)
		    need += sizeof(char *) + strlen(nametmp[i]) + 1;
		if ((ilist = (int *)malloc(sts * sizeof(insttmp[0]))) == NULL) {
		    PM_UNLOCK(ctxp->c_lock);
		    sts = -oserror();
		    goto pmapi_return;
		}
		if ((nlist = (char **)malloc(need)) == NULL) {
		    PM_UNLOCK(ctxp->c_lock);
		    sts = -oserror();
		    goto pmapi_return;
		}
		*instlist = ilist;
		*namelist = nlist;
		p = (char *)&nlist[sts];
		for (i = 0; i < sts; i++) {
		    ilist[i] = insttmp[i];
		    strcpy(p, nametmp[i]);
		    nlist[i] = p;
		    p += strlen(nametmp[i]) + 1;
		}
	    }
	}
	PM_UNLOCK(ctxp->c_lock);
    }

    if (sts == 0) {
	/* avoid ambiguity when no instances or errors */
	*instlist = NULL;
	*namelist = NULL;
    }

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
    if (sts <= 0) {
	/*
	 * clean up alloc's if errors or empty indoms and so not returning
	 * arrays via instlist[] and namelist[]
	 */
	if (ilist != NULL)
	    free(ilist);
	if (nlist != NULL)
	    free(nlist);
    }

    return sts;
}

void
__pmDumpInResult(FILE *f, const pmInResult *irp)
{
    int		i;
    char	strbuf[20];
    fprintf(f,"pmInResult dump from " PRINTF_P_PFX "%p for InDom %s (0x%x), numinst=%d\n",
		irp, pmInDomStr_r(irp->indom, strbuf, sizeof(strbuf)), irp->indom, irp->numinst);
    for (i = 0; i < irp->numinst; i++) {
	fprintf(f, "  [%d]", i);
	if (irp->instlist != NULL)
	    fprintf(f, " inst=%d", irp->instlist[i]);
	if (irp->namelist != NULL)
	    fprintf(f, " name=\"%s\"", irp->namelist[i]);
	fputc('\n', f);
    }
    return;
}

void
__pmFreeInResult(pmInResult *res)
{
    int		i;

    if (res->namelist != NULL) {
	for (i = 0; i < res->numinst; i++) {
	    if (res->namelist[i] != NULL) {
		free(res->namelist[i]);
	    }
	}
	free(res->namelist);
    }
    if (res->instlist != NULL)
	free(res->instlist);
    free(res);
}
