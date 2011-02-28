/*
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */

#include "pmapi.h"
#include "impl.h"

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

    if ((pp = (desc_req_t *)__pmFindPDUBuf(sizeof(desc_req_t))) == NULL)
	return -oserror();
    pp->hdr.len = sizeof(desc_req_t);
    pp->hdr.type = PDU_DESC_REQ;
    pp->hdr.from = from;
    pp->pmid = __htonpmID(pmid);

#ifdef DESPERATE
    fprintf(stderr, "__pmSendDescReq: converted 0x%08x (%s) to 0x%08x\n", pmid, pmIDStr(pmid), pp->pmid);
#endif

    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeDescReq(__pmPDU *pdubuf, pmID *pmid)
{
    desc_req_t	*pp;

    pp = (desc_req_t *)pdubuf;
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
    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeDesc(__pmPDU *pdubuf, pmDesc *desc)
{
    desc_t	*pp;

    pp = (desc_t *)pdubuf;
    desc->type = ntohl(pp->desc.type);
    desc->sem = ntohl(pp->desc.sem);
    desc->indom = __ntohpmInDom(pp->desc.indom);
    desc->units = __ntohpmUnits(pp->desc.units);
    desc->pmid = __ntohpmID(pp->desc.pmid);
    return 0;
}
