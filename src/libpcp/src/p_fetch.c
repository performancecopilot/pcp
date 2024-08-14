/*
 * Copyright (c) 2012-2013,2021-2022 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 * PDUs for pmFetch requests (PDU_FETCH, PDU_HIGHRES_FETCH)
 */
typedef struct {
    __pmPDUHdr		hdr;
    int			ctxid;		/* context slot index from the client */
    pmTimeval      	unused;		/* backward-compatibility, zeroed */
    int			numpmid;	/* number of PMIDs to follow */
    pmID		pmidlist[1];	/* one or more PMID(s) */
} fetch_t;

int
__pmSendFetchPDU(int fd, int from, int ctxid, int numpmid, pmID *pmidlist, int pdutype)
{
    size_t	need;
    fetch_t	*pp;
    int		j;
    int		sts;

    need = sizeof(fetch_t) + (numpmid-1) * sizeof(pmID);
    if ((pp = (fetch_t *)__pmFindPDUBuf((int)need)) == NULL)
	return -oserror();
    pp->hdr.len = (int)need;
    pp->hdr.type = pdutype;
    /* 
     * note: context id may be sent twice due to protocol evolution and
     * backwards compatibility issues
     */
    pp->hdr.from = from;
    pp->ctxid = htonl(ctxid);
    memset((void *)&pp->unused, 0, sizeof(pp->unused));
    pp->numpmid = htonl(numpmid);
    for (j = 0; j < numpmid; j++)
	pp->pmidlist[j] = __htonpmID(pmidlist[j]);

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmSendFetch(int fd, int from, int ctxid, int numpmid, pmID *pmidlist)
{
    return __pmSendFetchPDU(fd, from, ctxid, numpmid, pmidlist, PDU_FETCH);
}

int
__pmSendHighResFetch(int fd, int from, int ctxid, int numpmid, pmID *pmidlist)
{
    return __pmSendFetchPDU(fd, from, ctxid, numpmid, pmidlist, PDU_HIGHRES_FETCH);
}

static int
__pmDecodeFetchPDU(__pmPDU *pdubuf, int *ctxidp, int *numpmidp, pmID **pmidlist)
{
    fetch_t	*pp;
    char	*pduend;
    int		numpmid;
    int		j;

    pp = (fetch_t *)pdubuf;
    pduend = (char *)pdubuf + pp->hdr.len;

    if (pduend - (char*)pp < sizeof(fetch_t)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeFetchReq: PM_ERR_IPC: PDU remainder %d < sizeof(fetch_t) %d\n",
		(int)(pduend - (char*)pp), (int)sizeof(fetch_t));
	}
	return PM_ERR_IPC;
    }
    numpmid = ntohl(pp->numpmid);
    if (numpmid <= 0) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeFetchReq: PM_ERR_IPC: numpmid %d <= 0\n",
		numpmid);
	}
	return PM_ERR_IPC;
    }
    if ((pduend - (char *)pp) != sizeof(fetch_t) + ((sizeof(pmID)) * (numpmid-1))) {
	if (pmDebugOptions.pdu) {
	    if ((pduend - (char *)pp) > sizeof(fetch_t) + ((sizeof(pmID)) * (numpmid-1))) 
		fprintf(stderr, "__pmDecodeFetchReq: PM_ERR_IPC: PDU too long %d > sizeof(fetch_t) %d + sizeof(pmID) %d * (numpmid-1) %d\n",
		    (int)(pduend - (char*)pp), (int)sizeof(fetch_t), (int)sizeof(pmID), numpmid-1);
	    else
		fprintf(stderr, "__pmDecodeFetchReq: PM_ERR_IPC: PDU too short %d < sizeof(fetch_t) %d + sizeof(pmID) %d * (numpmid-1) %d\n",
		    (int)(pduend - (char*)pp), (int)sizeof(fetch_t), (int)sizeof(pmID), numpmid-1);
	}
	return PM_ERR_IPC;
    }

    for (j = 0; j < numpmid; j++)
	pp->pmidlist[j] = __ntohpmID(pp->pmidlist[j]);

    *ctxidp = ntohl(pp->ctxid);
    *numpmidp = numpmid;
    *pmidlist = pp->pmidlist;
    __pmPinPDUBuf((void *)pdubuf);
    return 0;
}

int
__pmDecodeFetch(__pmPDU *pdubuf, int *ctxidp, void *unused, int *numpmidp, pmID **pmidlist)
{
    memset(unused, 0, sizeof(((fetch_t *)0)->unused));
    return __pmDecodeFetchPDU(pdubuf, ctxidp, numpmidp, pmidlist);
}

int
__pmDecodeHighResFetch(__pmPDU *pdubuf, int *ctxidp, int *numpmidp, pmID **pmidlist)
{
    return __pmDecodeFetchPDU(pdubuf, ctxidp, numpmidp, pmidlist);
}
