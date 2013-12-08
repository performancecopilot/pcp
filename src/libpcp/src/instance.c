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
#include "impl.h"
#include "pmda.h"
#include "internal.h"

int
pmLookupInDom(pmInDom indom, const char *name)
{
    int			n;
    __pmInResult	*result;
    __pmContext		*ctxp;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;
    if ((n = pmWhichContext()) >= 0) {
	int	ctx = n;
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    PM_LOCK(ctxp->c_pmcd->pc_lock);
	    n = __pmSendInstanceReq(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp),
				    &ctxp->c_origin, indom, PM_IN_NULL, name);
	    if (n < 0)
		n = __pmMapErrno(n);
	    else {
		__pmPDU	*pb;
		int	pinpdu;
		pinpdu = n = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE, 
			       ctxp->c_pmcd->pc_tout_sec, &pb);
		if (n == PDU_INSTANCE) {
		    __pmInResult	*result;
		    if ((n = __pmDecodeInstance(pb, &result)) >= 0) {
			n = result->instlist[0];
			__pmFreeInResult(result);
		    }
		}
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
	    __pmDSO		*dp;
	    if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
		/* Local context requires single-threaded applications */
		n = PM_ERR_THREAD;
	    else if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
		/* We can safely cast away const here */
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dp->dispatch.version.four.ext->e_context = ctx;
		n = dp->dispatch.version.any.instance(indom, PM_IN_NULL, 
							(char *)name, &result, 
							dp->dispatch.version.any.ext);
	    }
	    if (n >= 0) {
		n = result->instlist[0];
		__pmFreeInResult(result);
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    n = __pmLogLookupInDom(ctxp->c_archctl->ac_log, indom, &ctxp->c_origin, name);
	}
	PM_UNLOCK(ctxp->c_lock);
    }

    return n;
}

int
pmNameInDom(pmInDom indom, int inst, char **name)
{
    int			n;
    __pmInResult	*result;
    __pmContext		*ctxp;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;
    if ((n = pmWhichContext()) >= 0) {
	int	ctx = n;
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    PM_LOCK(ctxp->c_pmcd->pc_lock);
	    n = __pmSendInstanceReq(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp),
				    &ctxp->c_origin, indom, inst, NULL);
	    if (n < 0)
		n = __pmMapErrno(n);
	    else {
		__pmPDU	*pb;
		int	pinpdu;
		pinpdu = n = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE,
					ctxp->c_pmcd->pc_tout_sec, &pb);
		if (n == PDU_INSTANCE) {
		    __pmInResult *result;
		    if ((n = __pmDecodeInstance(pb, &result)) >= 0) {
			if ((*name = strdup(result->namelist[0])) == NULL)
			    n = -oserror();
			__pmFreeInResult(result);
		    }
		}
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
	    __pmDSO	*dp;
	    if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
		/* Local context requires single-threaded applications */
		n = PM_ERR_THREAD;
	    else if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dp->dispatch.version.four.ext->e_context = ctx;
		n = dp->dispatch.version.any.instance(indom, inst, NULL, &result, dp->dispatch.version.any.ext);
	    }
	    if (n >= 0) {
		if ((*name = strdup(result->namelist[0])) == NULL)
		    n = -oserror();
		__pmFreeInResult(result);
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    char	*tmp;
	    if ((n = __pmLogNameInDom(ctxp->c_archctl->ac_log, indom, &ctxp->c_origin, inst, &tmp)) >= 0) {
		if ((*name = strdup(tmp)) == NULL)
		    n = -oserror();
	    }
	}
	PM_UNLOCK(ctxp->c_lock);
    }

    return n;
}

static int
inresult_to_lists(__pmInResult *result, int **instlist, char ***namelist)
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
    int			n;
    int			i;
    __pmInResult	*result;
    __pmContext		*ctxp;
    char		*p;
    int			need;
    int			*ilist;
    char		**nlist;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((n = pmWhichContext()) >= 0) {
	int	ctx = n;
	ctxp = __pmHandleToPtr(ctx);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    PM_LOCK(ctxp->c_pmcd->pc_lock);
	    n = __pmSendInstanceReq(ctxp->c_pmcd->pc_fd, __pmPtrToHandle(ctxp),
				    &ctxp->c_origin, indom, PM_IN_NULL, NULL);
	    if (n < 0)
		n = __pmMapErrno(n);
	    else {
		__pmPDU	*pb;
		int	pinpdu;
		pinpdu = n = __pmGetPDU(ctxp->c_pmcd->pc_fd, ANY_SIZE,
					ctxp->c_pmcd->pc_tout_sec, &pb);
		if (n == PDU_INSTANCE) {
		    __pmInResult	*result;
		    if ((n = __pmDecodeInstance(pb, &result)) < 0) {
			if (pinpdu > 0)
			    __pmUnpinPDUBuf(pb);
			PM_UNLOCK(ctxp->c_pmcd->pc_lock);
			return n;
		    }
		    n = inresult_to_lists(result, instlist, namelist);
		}
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
	    __pmDSO	*dp;
	    if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
		/* Local context requires single-threaded applications */
		n = PM_ERR_THREAD;
	    else if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dp->dispatch.version.four.ext->e_context = ctx;
		n = dp->dispatch.version.any.instance(indom, PM_IN_NULL, NULL,
					       &result,
					       dp->dispatch.version.any.ext);
	    }
	    if (n >= 0)
		n = inresult_to_lists(result, instlist, namelist);
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    int		*insttmp;
	    char	**nametmp;
	    if ((n = __pmLogGetInDom(ctxp->c_archctl->ac_log, indom, &ctxp->c_origin, &insttmp, &nametmp)) >= 0) {
		need = 0;
		for (i = 0; i < n; i++)
		    need += sizeof(char *) + strlen(nametmp[i]) + 1;
		if ((ilist = (int *)malloc(n * sizeof(insttmp[0]))) == NULL) {
		    PM_UNLOCK(ctxp->c_lock);
		    return -oserror();
		}
		if ((nlist = (char **)malloc(need)) == NULL) {
		    free(ilist);
		    PM_UNLOCK(ctxp->c_lock);
		    return -oserror();
		}
		*instlist = ilist;
		*namelist = nlist;
		p = (char *)&nlist[n];
		for (i = 0; i < n; i++) {
		    ilist[i] = insttmp[i];
		    strcpy(p, nametmp[i]);
		    nlist[i] = p;
		    p += strlen(nametmp[i]) + 1;
		}
	    }
	}
	PM_UNLOCK(ctxp->c_lock);
    }

    if (n == 0) {
	/* avoid ambiguity when no instances or errors */
	*instlist = NULL;
	*namelist = NULL;
    }

    return n;
}

#ifdef PCP_DEBUG
void
__pmDumpInResult(FILE *f, const __pmInResult *irp)
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
#endif

void
__pmFreeInResult(__pmInResult *res)
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
