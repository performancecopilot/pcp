/*
 * Copyright (c) 2012-2013,2022 Red Hat.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "internal.h"

/*
 * PDU for __pmControlLogging request (PDU_LOG_CONTROL)
 */

typedef struct {
    pmID		v_pmid;
    int			v_numval;	/* no. of vlist els to follow */
    __pmValue_PDU	v_list[1];	/* one or more */
} vlist_t;

typedef struct {
    __pmPDUHdr		c_hdr;
    int			c_control;	/* mandatory or advisory */
    int			c_state;	/* off, maybe or on */
    int			c_delta;	/* requested logging interval (msec) */
    int			c_numpmid;	/* no. of vlist_ts to follow */
    __pmPDU		c_data[1];	/* one or more */
} control_req_t;

int
__pmSendLogControl(int fd, const __pmResult *request, int control, int state, int delta)
{
    pmValueSet		*vsp;
    int			i;
    int			j;
    control_req_t	*pp;
    int			need;
    vlist_t		*vp;
    int			sts;

    if (pmDebugOptions.pdu)
	__pmPrintResult(stderr, request);

    /* advisory+maybe logging and retrospective logging (delta < 0) are not
     *permitted
     */
    if (delta < 0 ||
	(control == PM_LOG_ADVISORY && state == PM_LOG_MAYBE))
	return -EINVAL;

    /* PDU header, control, state and count of metrics */
    need = sizeof(control_req_t) - sizeof(pp->c_data);
    for (i = 0; i < request->numpmid; i++) {
	/* plus PMID and count of values */
	if (request->vset[i]->numval > 0)
	    need += sizeof(vlist_t) + (request->vset[i]->numval - 1)*sizeof(__pmValue_PDU);
	else
	    need += sizeof(vlist_t) - sizeof(__pmValue_PDU);
    }
    if ((pp = (control_req_t *)__pmFindPDUBuf(need)) == NULL)
	return -oserror();
    pp->c_hdr.len = need;
    pp->c_hdr.type = PDU_LOG_CONTROL;
    pp->c_hdr.from = FROM_ANON;		/* context does not matter here */
    pp->c_control = htonl(control);
    pp->c_state = htonl(state);
    pp->c_delta = htonl(delta);
    pp->c_numpmid = htonl(request->numpmid);
    vp = (vlist_t *)pp->c_data;
    for (i = 0; i < request->numpmid; i++) {
	vsp = request->vset[i];
	vp->v_pmid = __htonpmID(vsp->pmid);
	vp->v_numval = htonl(vsp->numval);
	/*
	 * Note: spec says only PM_VAL_INSITU can be used ... we don't
	 * check vsp->valfmt -- this is OK, because the "value" is never
	 * looked at!
	 */
	for (j = 0; j < vsp->numval; j++) {
	    vp->v_list[j].inst = htonl(vsp->vlist[j].inst);
	    vp->v_list[j].value.lval = htonl(vsp->vlist[j].value.lval);
	}
	if (vsp->numval > 0)
	    vp = (vlist_t *)((__psint_t)vp + sizeof(*vp) + (vsp->numval-1)*sizeof(__pmValue_PDU));
	else
	    vp = (vlist_t *)((__psint_t)vp + sizeof(*vp) - sizeof(__pmValue_PDU));
    }
    
    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeLogControl(const __pmPDU *pdubuf, __pmResult **request, int *control, int *state, int *delta)
{
    int			sts;
    int			i;
    int			j;
    int			nv;
    const control_req_t	*pp;
    char		*pduend;
    int			numpmid;
    int			maxnumpmid;
    size_t		need;
    __pmResult		*req;
    pmValueSet		*vsp;
    vlist_t		*vp;

    pp = (const control_req_t *)pdubuf;
    pduend = (char *)pdubuf + pp->c_hdr.len;
    /*
     * c_data has at least on pmid, so a minimal 2 words for
     * v_pmid and v_numval, less the 1 word for c_data
     */
    if (pp->c_hdr.len < sizeof(control_req_t) + sizeof(__pmPDU)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeLogControl: PM_ERR_IPC: short PDU %d < min size %d\n",
		pp->c_hdr.len, (int)(sizeof(control_req_t) + sizeof(__pmPDU)));
	}
	return PM_ERR_IPC;
    }

    *control = ntohl(pp->c_control);
    *state = ntohl(pp->c_state);
    *delta = ntohl(pp->c_delta);
    numpmid = ntohl(pp->c_numpmid);
    vp = (vlist_t *)pp->c_data;

    if (numpmid < 0) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeLogControl: PM_ERR_IPC: numpmid %d < 0\n",
		numpmid);
	}
	return PM_ERR_IPC;
    }
    /* v_numval may be 0, so v_vlist[] not there => min 2 words per pmid */
    maxnumpmid = (int)((pp->c_hdr.len - sizeof(control_req_t) + sizeof(__pmPDU)) / (2 * sizeof(__pmPDU)));
    if (numpmid > maxnumpmid) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeLogControl: PM_ERR_IPC: numpmid %d > max %d for PDU len %d\n",
		numpmid, maxnumpmid, pp->c_hdr.len);
	}
	return PM_ERR_IPC;
	}
    if ((req = __pmAllocResult(numpmid)) == NULL) {
	pmNoMem("__pmDecodeLogControl.req", sizeof(__pmResult) + (numpmid - 1) * sizeof(pmValueSet *), PM_RECOV_ERR);
	return -oserror();
    }
    req->numpmid = numpmid;

    sts = PM_ERR_IPC;
    for (i = 0; i < numpmid; i++) {
	/* check that numval field is within the input buffer */
	if (pduend - (char *)vp < sizeof(vlist_t) - sizeof(__pmValue_PDU)) {
	    goto corrupt;
	}
	nv = (int)ntohl(vp->v_numval);
	if (nv > pp->c_hdr.len) {
	    goto corrupt;
	}
	if (nv <= 0) {
	    need = sizeof(pmValueSet) - sizeof(pmValue);
	    /* check that pointer cannot move beyond input buffer end */
	    if (pduend - (char *)vp < sizeof(vlist_t) - sizeof(__pmValue_PDU)) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmDecodeLogControl: PM_ERR_IPC: remainder %d < sizeof(vlist_t) %d - sizeof(__pmValue_PDU) %d\n",
			(int)(pduend - (char *)vp), (int)sizeof(vlist_t), (int)sizeof(__pmValue_PDU));
		}
		goto corrupt;
	    }
	} else {
	    /* check that dynamic allocation argument will not wrap */
	    need = sizeof(pmValueSet) + ((nv - 1) * sizeof(pmValue));
	    /* check that pointer cannot move beyond input buffer end */
	    if (nv >= (pp->c_hdr.len - sizeof(vlist_t)) / sizeof(__pmValue_PDU)) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmDecodeLogControl: PM_ERR_IPC: pmid[%d] numval %d > max %d for PDU len %d\n",
			i, nv, (int)((pp->c_hdr.len - sizeof(vlist_t)) / sizeof(__pmValue_PDU)), pp->c_hdr.len);
		}
		goto corrupt;
	    }
	    if (pduend - (char *)vp < sizeof(vlist_t) + ((nv - 1) * sizeof(__pmValue_PDU))) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmDecodeLogControl: PM_ERR_IPC: remainder %d < sizeof(vlist_t) %d + (nv-1) %d * sizeof(__pmValue_PDU) %d\n",
			(int)(pduend - (char *)vp), (int)sizeof(vlist_t), nv-1, (int)sizeof(__pmValue_PDU));
		}
		goto corrupt;
	    }
	}
	if ((vsp = (pmValueSet *)malloc(need)) == NULL) {
	    pmNoMem("__pmDecodeLogControl.vsp", need, PM_RECOV_ERR);
	    sts = -oserror();
	    goto corrupt;
	}
	req->vset[i] = vsp;
	vsp->pmid = __ntohpmID(vp->v_pmid);
	vsp->valfmt = PM_VAL_INSITU;
	vsp->numval = nv;
	for (j = 0; j < nv; j++) {
	    vsp->vlist[j].inst = ntohl(vp->v_list[j].inst);
	    vsp->vlist[j].value.lval = ntohl(vp->v_list[j].value.lval);
	}
	if (nv > 0)
	    vp = (vlist_t *)((__psint_t)vp + sizeof(*vp) + (nv - 1)*sizeof(__pmValue_PDU));
	else
	    vp = (vlist_t *)((__psint_t)vp + sizeof(*vp) - sizeof(__pmValue_PDU));
    }

    *request = req;
    return 0;

corrupt:
    /* req->vset[0] ... req->vset[i-1] have been malloc'd */
    for (i-- ; i >= 0; i--)
	free(req->vset[i]);
    req->numpmid = 0;		/* don't free vset's */
    __pmFreeResult(req);
    return sts;
}
