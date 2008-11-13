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
__pmSendDescReq(int fd, int mode, pmID pmid)
{
    desc_req_t	*pp;

    if (mode == PDU_BINARY) {
	if ((pp = (desc_req_t *)__pmFindPDUBuf(sizeof(desc_req_t))) == NULL)
	    return -errno;
	pp->hdr.len = sizeof(desc_req_t);
	pp->hdr.type = PDU_DESC_REQ;
	pp->pmid = __htonpmID(pmid);

#ifdef DESPERATE
	fprintf(stderr, "__pmSendDescReq: converted 0x%08x (%s) to 0x%08x\n", pmid, pmIDStr(pmid), pp->pmid);
#endif

	return __pmXmitPDU(fd, (__pmPDU *)pp);
    }
    else {
	int		nbytes, sts;

	snprintf(__pmAbuf, sizeof(__pmAbuf), "DESC_REQ %d\n", pmid);
	nbytes = (int)strlen(__pmAbuf);
	sts = __pmXmitAscii(fd, __pmAbuf, nbytes);
	if (sts < 0)
	    return sts;
	return 0;
    }

}

int
__pmDecodeDescReq(__pmPDU *pdubuf, int mode, pmID *pmid)
{
    desc_req_t	*pp;

    if (mode == PDU_ASCII) {
	/* Incoming ASCII request PDUs not supported */
	return PM_ERR_NOASCII;
    }

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
__pmSendDesc(int fd, int mode, pmDesc *desc)
{
    desc_t		*pp;

    if (mode == PDU_ASCII) {
	/* Outgoing ASCII result PDUs not supported */
	return PM_ERR_NOASCII;
    }

    if ((pp = (desc_t *)__pmFindPDUBuf(sizeof(desc_t))) == NULL)
	return -errno;

    pp->hdr.len = sizeof(desc_t);
    pp->hdr.type = PDU_DESC;

    pp->desc.type = htonl(desc->type);
    pp->desc.sem = htonl(desc->sem);

    pp->desc.indom = __htonpmInDom(desc->indom);
    pp->desc.units = __htonpmUnits(desc->units);
    pp->desc.pmid = __htonpmID(desc->pmid);

    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeDesc(__pmPDU *pdubuf, int mode, pmDesc *desc)
{
    desc_t		*pp;

    if (mode == PDU_BINARY) {
	pp = (desc_t *)pdubuf;
	desc->type = ntohl(pp->desc.type);
	desc->sem = ntohl(pp->desc.sem);

	desc->indom = __ntohpmInDom(pp->desc.indom);
	desc->units = __ntohpmUnits(pp->desc.units);
	desc->pmid = __ntohpmID(pp->desc.pmid);
	return 0;
    }
    else {
	/* assume PDU_ASCII */
	int		n;
	int		dimSpace, dimTime, dimCount;
	int		scaleSpace, scaleTime, scaleCount;

	n = __pmRecvLine(pdubuf, ABUFSIZE, __pmAbuf);
	if (n <= 0)
	    return n;
	if ((n = sscanf(__pmAbuf, "DESC %d %d %d %d %d %d %d %d %d %d",
	    &desc->pmid, &desc->type, &desc->indom, &desc->sem,
	    &dimSpace, &dimTime, &dimCount,
	    &scaleSpace, &scaleTime, &scaleCount)) != 10) {
	    __pmNotifyErr(LOG_WARNING, "__pmDecodeDesc: ASCII botch %d values from: \"%s\"\n", n, __pmAbuf);
	    return PM_ERR_IPC;
	}
	desc->units.dimSpace = dimSpace;
	desc->units.dimTime = dimTime;
	desc->units.dimCount = dimCount;
	desc->units.scaleSpace = scaleSpace;
	desc->units.scaleTime = scaleTime;
	desc->units.scaleCount = scaleCount;
	return 0;
    }
}
