/*
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

static int
request_instance (__pmContext *ctxp, pmInDom indom, int inst, const char *name)
{
    int n;

    if (ctxp->c_pmcd->pc_curpdu != 0) {
	return (PM_ERR_CTXBUSY);
    }
    
    n = __pmSendInstanceReq(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
				&ctxp->c_origin, indom, inst, name);
    if (n < 0) {
	n = __pmMapErrno(n);
    }

    return (n);
}

static int
ctxid_request_instance (int ctx,  pmInDom indom, int inst, const char *name)
{
    int n;
    __pmContext *ctxp;

    if ((n = __pmGetHostContextByID(ctx, &ctxp)) >= 0) {
	if ((n = request_instance (ctxp, indom, inst, name)) >= 0) {
	    ctxp->c_pmcd->pc_curpdu = PDU_INSTANCE_REQ;
	    ctxp->c_pmcd->pc_tout_sec = TIMEOUT_DEFAULT;
	}
    }

    return (n);
}

int
pmRequestInDomInst (int ctx, pmInDom indom, const char *name)
{
    return (ctxid_request_instance(ctx, indom, PM_IN_NULL, name));
}

static int
receive_instance_id (__pmContext *ctxp)
{
    int n;
    __pmPDU	*pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY, 
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_INSTANCE) {
	__pmInResult	*result;
	
	if ((n = __pmDecodeInstance(pb, PDU_BINARY, &result)) >= 0) {
	    n = result->instlist[0];
	    __pmFreeInResult(result);
	}
    }
    else if (n == PDU_ERROR)
	__pmDecodeError(pb, PDU_BINARY, &n);
    else if (n != PM_ERR_TIMEOUT)
	n = PM_ERR_IPC;

    return (n);
}

int 
pmReceiveInDomInst (int ctx)
{
    int n;
    __pmContext	*ctxp;

    if ((n = __pmGetBusyHostContextByID(ctx, &ctxp, PDU_INSTANCE_REQ)) >= 0) {
	n = receive_instance_id(ctxp);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }
    return (n);
}

int
pmLookupInDom(pmInDom indom, const char *name)
{
    int		n;
    __pmInResult	*result;
    __pmContext	*ctxp;
    __pmDSO	*dp;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;
    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);

	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if ((n = request_instance (ctxp, indom, PM_IN_NULL, name)) >= 0) {
		n = receive_instance_id (ctxp);
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
		/* We can safely cast away const here */
		if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    n = dp->dispatch.version.one.instance(indom, PM_IN_NULL, 
							  (char *)name, 
							  &result);
		else
		    n = dp->dispatch.version.two.instance(indom, PM_IN_NULL, 
							  (char *)name, 
							  &result, 
							  dp->dispatch.version.two.ext);
		if (n < 0 && dp->dispatch.comm.pmapi_version == PMAPI_VERSION_1)
		    n = XLATE_ERR_1TO2(n);
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
    }

    return n;
}

int
pmRequestInDomName (int ctx, pmInDom indom, int inst)
{
    return (ctxid_request_instance (ctx, indom, inst, NULL));
}

static int
receive_instance_name (__pmContext *ctxp, char **name)
{
    int n;
    __pmPDU *pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY,
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_INSTANCE) {
	__pmInResult *result;
	
	if ((n = __pmDecodeInstance(pb, PDU_BINARY, &result)) >= 0) {
	    if ((*name = strdup(result->namelist[0]))== NULL)
		n = (errno) ? -errno : -ENOMEM;
	    __pmFreeInResult(result);
	}
    }
    else if (n == PDU_ERROR)
	__pmDecodeError(pb, PDU_BINARY, &n);
    else if (n != PM_ERR_TIMEOUT)
	n = PM_ERR_IPC;

    return (n);
}

int
pmReceiveInDomName (int ctx, char **name)
{
    int n;
    __pmContext	*ctxp;

    if ((n = __pmGetBusyHostContextByID(ctx, &ctxp, PDU_INSTANCE_REQ)) >= 0) {
	n = receive_instance_name (ctxp, name);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }
    return (n);
}

int
pmNameInDom(pmInDom indom, int inst, char **name)
{
    int		n;
    __pmInResult	*result;
    __pmContext	*ctxp;
    __pmDSO	*dp;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;
    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if ((n = request_instance(ctxp, indom, inst, NULL)) >= 0) {
		n = receive_instance_name (ctxp, name);
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    n = dp->dispatch.version.one.instance(indom, inst, NULL, &result);
		else
		    n = dp->dispatch.version.two.instance(indom, inst, NULL, &result, dp->dispatch.version.two.ext);
		if (n < 0 &&
		    dp->dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			n = XLATE_ERR_1TO2(n);
	    }
	    if (n >= 0) {
		if ((*name = strdup(result->namelist[0])) == NULL)
		    n = -errno;
		__pmFreeInResult(result);
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    char	*tmp;
	    if ((n = __pmLogNameInDom(ctxp->c_archctl->ac_log, indom, &ctxp->c_origin, inst, &tmp)) >= 0) {
		if ((*name = strdup(tmp)) == NULL)
		    n = -errno;
	    }
	}
    }

    return n;
}

int
pmRequestInDom (int ctx, pmInDom indom)
{
    return (ctxid_request_instance (ctx, indom, PM_IN_NULL, NULL));
}

static int
inresult_to_lists (__pmInResult *result, int **instlist, char ***namelist)
{
    int n, i, need;
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
    if ((ilist = (int *)malloc(result->numinst * sizeof(result->instlist[0]))) == NULL) {
	__pmFreeInResult(result);
	return ((errno) ? -errno : -ENOMEM);
    }
    if ((nlist = (char **)malloc(need)) == NULL) {
	free(ilist);
	__pmFreeInResult(result);
	return ((errno) ? -errno : -ENOMEM);
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
    return (n);
}

int
receive_indom (__pmContext *ctxp,  int **instlist, char ***namelist)
{
    int n;
    __pmPDU	*pb;

    n = __pmGetPDU(ctxp->c_pmcd->pc_fd, PDU_BINARY,
		   ctxp->c_pmcd->pc_tout_sec, &pb);
    if (n == PDU_INSTANCE) {
	__pmInResult	*result;
	    
	if ((n = __pmDecodeInstance(pb, PDU_BINARY, &result)) < 0)
	    return n;

	n = inresult_to_lists (result, instlist, namelist);
    }
    else if (n == PDU_ERROR)
	__pmDecodeError(pb, PDU_BINARY, &n);
    else if (n != PM_ERR_TIMEOUT)
	n = PM_ERR_IPC;

    return (n);
}

int
pmReceiveInDom (int ctx,  int **instlist, char ***namelist)
{
    int n;
    __pmContext	*ctxp;

    if ((n = __pmGetBusyHostContextByID(ctx, &ctxp,  PDU_INSTANCE_REQ)) >= 0) {
	n = receive_indom (ctxp, instlist, namelist);

	ctxp->c_pmcd->pc_curpdu = 0;
	ctxp->c_pmcd->pc_tout_sec = 0;
    }
    return (n);
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
    __pmDSO		*dp;

    /* avoid ambiguity when no instances or errors */
    *instlist = NULL;
    *namelist = NULL;
    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp->c_type == PM_CONTEXT_HOST) {
	    if ((n = request_instance (ctxp, indom, PM_IN_NULL, NULL)) >= 0) {
		n = receive_indom (ctxp, instlist, namelist);
	    }
	}
	else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	    if ((dp = __pmLookupDSO(((__pmInDom_int *)&indom)->domain)) == NULL)
		n = PM_ERR_NOAGENT;
	    else {
		if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    n = dp->dispatch.version.one.instance(indom, PM_IN_NULL, NULL,
					      &result);
		else
		    n = dp->dispatch.version.two.instance(indom, PM_IN_NULL, NULL,
					       &result,
					       dp->dispatch.version.two.ext);
		if (n < 0 &&
		    dp->dispatch.comm.pmapi_version == PMAPI_VERSION_1)
			n = XLATE_ERR_1TO2(n);
	    }
	    if (n >= 0) {
		n = inresult_to_lists (result, instlist, namelist);
	    }
	}
	else {
	    /* assume PM_CONTEXT_ARCHIVE */
	    int		*insttmp;
	    char	**nametmp;
	    if ((n = __pmLogGetInDom(ctxp->c_archctl->ac_log, indom, &ctxp->c_origin, &insttmp, &nametmp)) >= 0) {
		need = 0;
		for (i = 0; i < n; i++) {
		    need += sizeof(char *) + strlen(nametmp[i]) + 1;
		}
		if ((ilist = (int *)malloc(n * sizeof(insttmp[0]))) == NULL) {
		    return -errno;
		}
		if ((nlist = (char **)malloc(need)) == NULL) {
		    free(ilist);
		    return -errno;
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
    }

    return n;
}

#ifdef PCP_DEBUG
void
__pmDumpInResult(FILE *f, const __pmInResult *irp)
{
    int		i;
    fprintf(f,"pmInResult dump from " PRINTF_P_PFX "%p for InDom %s (0x%x), numinst=%d\n",
		irp, pmInDomStr(irp->indom), irp->indom, irp->numinst);
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
