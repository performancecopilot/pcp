/*
 * Copyright (c) 2012-2013 Red Hat.
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
    char	*pduend;

    pp = (desc_req_t *)pdubuf;
    pduend = (char *)pdubuf + pp->hdr.len;

    if (pduend - (char*)pp != sizeof(desc_req_t))
	return PM_ERR_IPC;

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
    char	*pduend;

    pp = (desc_t *)pdubuf;
    pduend = (char *)pdubuf + pp->hdr.len;

    if (pduend - (char*)pp != sizeof(desc_t))
	return PM_ERR_IPC;

    desc->type = ntohl(pp->desc.type);
    desc->sem = ntohl(pp->desc.sem);
    desc->indom = __ntohpmInDom(pp->desc.indom);
    desc->units = __ntohpmUnits(pp->desc.units);
    desc->pmid = __ntohpmID(pp->desc.pmid);
    return 0;
}
