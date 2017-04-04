/*
 * Copyright (c) 2012-2013 Red Hat.
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
#include "impl.h"
#include "internal.h"

/*
 * PDU for pmFetch request (PDU_FETCH)
 */
typedef struct {
    __pmPDUHdr		hdr;
    int			ctxnum;		/* context no. */
    __pmTimeval      	when;		/* desired time */
    int			numpmid;	/* no. PMIDs to follow */
    pmID		pmidlist[1];	/* one or more */
} fetch_t;

int
__pmSendFetch(int fd, int from, int ctxnum, __pmTimeval *when, int numpmid, pmID *pmidlist)
{
    size_t	need;
    fetch_t	*pp;
    int		j;
    int		sts;

    need = sizeof(fetch_t) + (numpmid-1) * sizeof(pmID);
    if ((pp = (fetch_t *)__pmFindPDUBuf((int)need)) == NULL)
	return -oserror();
    pp->hdr.len = (int)need;
    pp->hdr.type = PDU_FETCH;
    /* 
     * note: context id may be sent twice due to protocol evolution and
     * backwards compatibility issues
     */
    pp->hdr.from = from;
    pp->ctxnum = htonl(ctxnum);
    if (when == NULL)
	memset((void *)&pp->when, 0, sizeof(pp->when));
    else {
#if defined(HAVE_32BIT_LONG)
	pp->when.tv_sec = htonl(when->tv_sec);
	pp->when.tv_usec = htonl(when->tv_usec);
#else
	pp->when.tv_sec = htonl((__int32_t)when->tv_sec);
	pp->when.tv_usec = htonl((__int32_t)when->tv_usec);
#endif
    }
    pp->numpmid = htonl(numpmid);
    for (j = 0; j < numpmid; j++)
	pp->pmidlist[j] = __htonpmID(pmidlist[j]);

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeFetch(__pmPDU *pdubuf, int *ctxnum, __pmTimeval *when, int *numpmidp, pmID **pmidlist)
{
    fetch_t	*pp;
    char	*pduend;
    int		numpmid;
    int		j;

    pp = (fetch_t *)pdubuf;
    pduend = (char *)pdubuf + pp->hdr.len;

    if (pduend - (char*)pp < sizeof(fetch_t))
	return PM_ERR_IPC;
    numpmid = ntohl(pp->numpmid);
    if (numpmid <= 0 || numpmid > pp->hdr.len)
	return PM_ERR_IPC;
    if (numpmid >= (INT_MAX - sizeof(fetch_t)) / sizeof(pmID))
	return PM_ERR_IPC;
    if ((pduend - (char*)pp) != sizeof(fetch_t) + ((sizeof(pmID)) * (numpmid-1)))
	return PM_ERR_IPC;

    for (j = 0; j < numpmid; j++)
	pp->pmidlist[j] = __ntohpmID(pp->pmidlist[j]);

    *ctxnum = ntohl(pp->ctxnum);
    when->tv_sec = ntohl(pp->when.tv_sec);
    when->tv_usec = ntohl(pp->when.tv_usec);
    *numpmidp = numpmid;
    *pmidlist = pp->pmidlist;
    __pmPinPDUBuf((void *)pdubuf);
    return 0;
}
