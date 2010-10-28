/*
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */

#include "pmapi.h"
#include "impl.h"

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

    need = sizeof(fetch_t) + (numpmid-1) * sizeof(pmID);
    if ((pp = (fetch_t *)__pmFindPDUBuf((int)need)) == NULL)
	return -errno;
    pp->hdr.len = (int)need;
    pp->hdr.type = PDU_FETCH;
    /* 
     * note: context id may be send twice due to protocol evolution and
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
    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeFetch(__pmPDU *pdubuf, int mode, int *ctxnum, __pmTimeval *when, int *numpmid, pmID **pmidlist)
{
    fetch_t	*pp;
    int		j;

    if (mode == PDU_ASCII)
	return PM_ERR_NOASCII;
    pp = (fetch_t *)pdubuf;
    *ctxnum = ntohl(pp->ctxnum);
    when->tv_sec = ntohl(pp->when.tv_sec);
    when->tv_usec = ntohl(pp->when.tv_usec);
    *numpmid = ntohl(pp->numpmid);
    for (j = 0; j < *numpmid; j++)
	pp->pmidlist[j] = __ntohpmID(pp->pmidlist[j]);
    *pmidlist = pp->pmidlist;
    __pmPinPDUBuf((void *)pdubuf);
    return 0;
}
