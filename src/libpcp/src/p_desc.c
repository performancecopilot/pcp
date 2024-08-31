/*
 * Copyright (c) 2012-2013,2021 Red Hat.
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * PDU for pmLookupDesc request (PDU_DESC_REQ)
 */
typedef struct {
    __pmPDUHdr	hdr;
    pmID	pmid;
} desc_req_t;

int
__pmSendDescReq(int fd, int from, pmID pmid)
{
    desc_req_t	*pp;
    int		sts;

    if ((pp = (desc_req_t *)__pmFindPDUBuf(sizeof(desc_req_t))) == NULL)
	return -oserror();
    pp->hdr.len = sizeof(desc_req_t);
    pp->hdr.type = PDU_DESC_REQ;
    pp->hdr.from = from;
    pp->pmid = __htonpmID(pmid);

#ifdef DESPERATE
    {
	char	strbuf[20];
	fprintf(stderr, "__pmSendDescReq: converted 0x%08x (%s) to 0x%08x\n", pmid, pmIDStr_r(pmid, strbuf, sizeof(strbuf)), pp->pmid);
    }
#endif

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeDescReq(__pmPDU *pdubuf, pmID *pmid)
{
    desc_req_t	*pp;

    pp = (desc_req_t *)pdubuf;

    if (pp->hdr.len != sizeof(desc_req_t)) {
	if (pmDebugOptions.pdu) {
	    char	*what;
	    char	op;
	    if (pp->hdr.len > sizeof(desc_req_t)) {
		what = "long";
		op = '>';
	    }
	    else {
		what = "short";
		op = '<';
	    }
	    fprintf(stderr, "__pmDecodeDescReq: PM_ERR_IPC: PDU too %s %d %c required size %d\n",
			    what, pp->hdr.len, op, (int)sizeof(desc_req_t));
	}
	return PM_ERR_IPC;
    }

    *pmid = __ntohpmID(pp->pmid);
    return 0;
}

/*
 * PDU for pmLookupDesc result (PDU_DESC)
 */
typedef struct {
    __pmPDUHdr	hdr;
    pmDesc	desc;
} desc_t;

int
__pmSendDesc(int fd, int ctx, pmDesc *desc)
{
    desc_t	*pp;
    int		sts;

    if ((pp = (desc_t *)__pmFindPDUBuf(sizeof(desc_t))) == NULL)
	return -oserror();

    pp->hdr.len = sizeof(desc_t);
    pp->hdr.type = PDU_DESC;
    pp->hdr.from = ctx;
    pp->desc.type = htonl(desc->type);
    pp->desc.sem = htonl(desc->sem);
    pp->desc.indom = __htonpmInDom(desc->indom);
    pp->desc.units = __htonpmUnits(desc->units);
    pp->desc.pmid = __htonpmID(desc->pmid);

    sts =__pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeDesc(__pmPDU *pdubuf, pmDesc *desc)
{
    desc_t	*pp;

    pp = (desc_t *)pdubuf;

    if (pp->hdr.len != sizeof(desc_t)) {
	if (pmDebugOptions.pdu) {
	    char	*what;
	    char	op;
	    if (pp->hdr.len > sizeof(desc_t)) {
		what = "long";
		op = '>';
	    }
	    else {
		what = "short";
		op = '<';
	    }
	    fprintf(stderr, "__pmDecodeDesc: PM_ERR_IPC: PDU too %s %d %c required size %d\n",
			    what, pp->hdr.len, op, (int)sizeof(desc_t));
	}
	return PM_ERR_IPC;
    }

    desc->type = ntohl(pp->desc.type);
    desc->sem = ntohl(pp->desc.sem);
    desc->indom = __ntohpmInDom(pp->desc.indom);
    desc->units = __ntohpmUnits(pp->desc.units);
    desc->pmid = __ntohpmID(pp->desc.pmid);
    return 0;
}

/*
 * PDU for pmLookupDescs result (PDU_DESCS)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		numdescs;
    pmDesc	desc[1];
} descs_t;

int
__pmSendDescs(int fd, int ctx, int numdescs, pmDesc *descs)
{
    descs_t	*pp;
    pmDesc	*dp;
    int		i;
    int		sts;
    int		need;

    if (numdescs <= 0)
	return -EINVAL;

    need = sizeof(descs_t) + (numdescs - 1) * sizeof(pmDesc);
    if ((pp = (descs_t *)__pmFindPDUBuf(need)) == NULL)
	return -oserror();

    pp->hdr.len = need;
    pp->hdr.type = PDU_DESCS;
    pp->hdr.from = ctx;
    pp->numdescs = htonl(numdescs);
    for (i = 0; i < numdescs; i++) {
	dp = &pp->desc[i];
	dp->type = htonl(descs[i].type);
	dp->sem = htonl(descs[i].sem);
	dp->indom = __htonpmInDom(descs[i].indom);
	dp->units = __htonpmUnits(descs[i].units);
	dp->pmid = __htonpmID(descs[i].pmid);
    }

    sts =__pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeDescs(__pmPDU *pdubuf, int numdescs, pmDesc *desclist)
{
    descs_t	*pp;
    int		total;
    int		count;
    int		i;
    int		need;

    pp = (descs_t *)pdubuf;

    if (pp->hdr.len < sizeof(descs_t)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeDescs: PM_ERR_IPC: short PDU %d < min size %d\n",
		pp->hdr.len,  (int)sizeof(descs_t));
	}
	return PM_ERR_IPC;
    }
    total = ntohl(pp->numdescs);
    if (total <= 0 || total != numdescs) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeDescs: PM_ERR_IPC: total %d <= 0 or != numdescs %d\n",
		total, numdescs);
	}
	return PM_ERR_IPC;
    }
    need = (int)(sizeof(descs_t) + (total - 1) * sizeof(pmDesc));
    if (pp->hdr.len != need) {
	if (pmDebugOptions.pdu) {
	    char	*what;
	    char	op;
	    if (pp->hdr.len > need) {
		what = "long";
		op = '>';
	    }
	    else {
		what = "short";
		op = '<';
	    }
	    fprintf(stderr, "__pmDecodeDescs: PM_ERR_IPC: PDU too %s %d %c required size %d\n",
			    what, pp->hdr.len, op, need);
	}
	return PM_ERR_IPC;
    }

    for (i = count = 0; i < total; i++) {
	desclist[i].type = ntohl(pp->desc[i].type);
	desclist[i].sem = ntohl(pp->desc[i].sem);
	desclist[i].indom = __ntohpmInDom(pp->desc[i].indom);
	desclist[i].units = __ntohpmUnits(pp->desc[i].units);
	desclist[i].pmid = __ntohpmID(pp->desc[i].pmid);
	if (desclist[i].pmid != PM_ID_NULL)
	    count++;
    }
    return count;
}

int
__pmDecodeDescs2(__pmPDU *pdubuf, int *numdescs, pmDesc **descs)
{
    descs_t	*pp;
    pmDesc	*desclist;
    int		total;
    int		count;
    int		i;
    int		need;

    pp = (descs_t *)pdubuf;

    if (pp->hdr.len < sizeof(descs_t)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeDescs2: PM_ERR_IPC: short PDU %d < min size %d\n",
		pp->hdr.len,  (int)sizeof(descs_t));
	}
	return PM_ERR_IPC;
    }
    total = ntohl(pp->numdescs);
    if (total <= 0 || total > (INT_MAX / sizeof(pmDesc))) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeDescs2: PM_ERR_IPC: total %d <= 0 or > (INT_MAX ...) %d\n",
		total, (int)(INT_MAX / sizeof(pmDesc)));
	}
	return PM_ERR_IPC;
    }
    need = (int)(sizeof(descs_t) + (total - 1) * sizeof(pmDesc));
    if (pp->hdr.len != need) {
	if (pmDebugOptions.pdu) {
	    char	*what;
	    char	op;
	    if (pp->hdr.len > need) {
		what = "long";
		op = '>';
	    }
	    else {
		what = "short";
		op = '<';
	    }
	    fprintf(stderr, "__pmDecodeDescs2: PM_ERR_IPC: PDU too %s %d %c required size %d\n",
			    what, pp->hdr.len, op, need);
	}
	return PM_ERR_IPC;
    }

    if ((desclist = malloc(total * sizeof(pmDesc))) == NULL)
	return -oserror();

    *descs = desclist;
    *numdescs = total;

    for (i = count = 0; i < total; i++) {
	desclist[i].type = ntohl(pp->desc[i].type);
	desclist[i].sem = ntohl(pp->desc[i].sem);
	desclist[i].indom = __ntohpmInDom(pp->desc[i].indom);
	desclist[i].units = __ntohpmUnits(pp->desc[i].units);
	desclist[i].pmid = __ntohpmID(pp->desc[i].pmid);
	if (desclist[i].pmid != PM_ID_NULL)
	    count++;
    }
    return count;
}
