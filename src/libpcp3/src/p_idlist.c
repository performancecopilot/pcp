/*
 * Copyright (c) 2012-2015,2021 Red Hat.
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

/*
 * PDU for id list (PDU_PMNS_IDS and PDU_DESC_IDS)
 */
typedef struct {
    __pmPDUHdr   hdr;
    int		sts;      /* never used, protocol remnant */
    int		numids;
    pmID        idlist[1];
} idlist_t;

void
__pmDumpIDList(FILE *f, int numids, const pmID idlist[])
{
    int		i;
    char	strbuf[20];

    fprintf(f, "IDlist dump: numids = %d\n", numids);
    for (i = 0; i < numids; i++)
	fprintf(f, "  PMID[%d]: 0x%08x %s\n", i, idlist[i], pmIDStr_r(idlist[i], strbuf, sizeof(strbuf)));
}

/*
 * Send PDU_PMNS_IDS or PDU_DESC_IDS across socket.
 */
int
__pmSendIDList(int fd, int from, int numids, const pmID idlist[], int sts)
{
    idlist_t	*ip;
    int		need;
    int		j;
    int		lsts;
    int		type = (sts == -1) ? PDU_DESC_IDS : PDU_PMNS_IDS;

    if (pmDebugOptions.pmns) {
	fprintf(stderr, "%s\n", "__pmSendIDList");
	__pmDumpIDList(stderr, numids, idlist);
    }

    need = (int)(sizeof(idlist_t) + (numids-1) * sizeof(idlist[0]));

    if ((ip = (idlist_t *)__pmFindPDUBuf(need)) == NULL)
	return -oserror();
    ip->hdr.len = need;
    ip->hdr.type = type;
    ip->hdr.from = from;
    ip->sts = htonl(sts);
    ip->numids = htonl(numids);
    for (j = 0; j < numids; j++) {
	ip->idlist[j] = __htonpmID(idlist[j]);
    }

    lsts = __pmXmitPDU(fd, (__pmPDU *)ip);
    __pmUnpinPDUBuf(ip);
    return lsts;
}

/*
 * Decode a PDU_PMNS_IDS.
 * Assumes that we have preallocated idlist prior to this call
 * (i.e. we know how many should definitely be coming over)
 * Returns 0 on success.
 */
int
__pmDecodeIDList(__pmPDU *pdubuf, int numids, pmID idlist[], int *sts)
{
    idlist_t	*pp;
    int		nids;
    int		j;
    int		need;

    pp = (idlist_t *)pdubuf;

    if (pp->hdr.len < sizeof(idlist_t)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeIDList: PM_ERR_IPC: short PDU %d < min size %d\n",
		pp->hdr.len,  (int)sizeof(idlist_t));
	}
	return PM_ERR_IPC;
    }
    *sts = ntohl(pp->sts);
    nids = ntohl(pp->numids);
    if (nids <= 0 || nids != numids) {
	if (pmDebugOptions.pdu) {
	    if (nids <= 0)
		fprintf(stderr, "__pmDecodeIDList: PM_ERR_IPC: numids from PDU %d <= 0\n",
		    nids);
	    else
		fprintf(stderr, "__pmDecodeIDList: PM_ERR_IPC: numids from PDU %d != numids from caller %d\n",
		    nids, numids);
	}
	return PM_ERR_IPC;
    }
    need = (int)(sizeof(idlist_t) + (sizeof(pmID) * (nids-1)));
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
	    fprintf(stderr, "__pmDecodeIDList: PM_ERR_IPC: PDU too %s %d %c required size %d\n",
		    what, pp->hdr.len, op, need);
	}
	return PM_ERR_IPC;
    }

    for (j = 0; j < numids; j++)
	idlist[j] = __ntohpmID(pp->idlist[j]);

    if (pmDebugOptions.pmns) {
	fprintf(stderr, "%s\n", "__pmDecodeIDList");
	__pmDumpIDList(stderr, numids, idlist);
    }

    return 0;
}

/*
 * Decode a PDU_DESC_IDS (variant #2)
 * We do not have a preallocated idlist prior to this call.
 * Returns 0 on success.
 */
int
__pmDecodeIDList2(__pmPDU *pdubuf, int *numids, pmID **idlist)
{
    idlist_t	*pp;
    pmID	*pmidlist;
    int		nids;
    int		maxnids;
    int		j;
    int		need;

    pp = (idlist_t *)pdubuf;

    if (pp->hdr.len < sizeof(idlist_t)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeIDList2: PM_ERR_IPC: short PDU %d < min size %d\n",
		pp->hdr.len,  (int)sizeof(idlist_t));
	}
	return PM_ERR_IPC;
    }
    if (ntohl(pp->sts) != -1) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeIDList2: PM_ERR_IPC: sts %d != -1\n",
		ntohl(pp->sts));
	}
	return PM_ERR_IPC;
    }
    nids = ntohl(pp->numids);
    if (nids <= 0) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeIDList2: PM_ERR_IPC: numids %d <= 0\n",
		nids);
	}
	return PM_ERR_IPC;
    }
    /* PDU size defines number of pmIDs allowed */
    maxnids = (int)((pp->hdr.len - sizeof(idlist_t) + sizeof(pmID)) / sizeof(pmID));
    if (nids > maxnids) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeIDList2: PM_ERR_IPC: numids %d > max %d for PDU len %d\n",
		nids, maxnids, pp->hdr.len);
	}
	return PM_ERR_IPC;
    }
    need = (int)(sizeof(idlist_t) + (sizeof(pmID) * (nids-1)));
    if (pp->hdr.len != need) {
	if (pmDebugOptions.pdu) {
	    char	*what;
	    char	op;
	    if (pp->hdr.len > need) {
		what = "long";
		op = '>';
	    }
	    else {
		/* cannot happen because of maxnids check above */
		what = "short";
		op = '<';
	    }
	    fprintf(stderr, "__pmDecodeIDList2: PM_ERR_IPC: PDU too %s %d %c required size %d\n",
		    what, pp->hdr.len, op, need);
	}
	return PM_ERR_IPC;
    }

    if ((pmidlist = malloc(nids * sizeof(pmDesc))) == NULL)
	return -oserror();
    for (j = 0; j < nids; j++)
	pmidlist[j] = __ntohpmID(pp->idlist[j]);

    if (pmDebugOptions.pmns) {
	fprintf(stderr, "%s\n", "__pmDecodeIDList2");
	__pmDumpIDList(stderr, nids, pmidlist);
    }

    *idlist = pmidlist;
    *numids = nids;
    return 0;
}
