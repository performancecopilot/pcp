/*
 * Copyright (c) 2012-2013 Red Hat.
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

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

#define LIMIT_CREDS	1024

/*
 * PDU for process credentials (PDU_CREDS)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		numcreds;
    __pmCred	credlist[1];
} creds_t;

int
__pmSendCreds(int fd, int from, int credcount, const __pmCred *credlist)
{
    size_t	need;
    creds_t	*pp;
    int		i;
    int		sts;

    if (credcount <= 0 || credcount > LIMIT_CREDS || credlist == NULL)
	return PM_ERR_IPC;

    need = sizeof(creds_t) + ((credcount-1) * sizeof(__pmCred));
    if ((pp = (creds_t *)__pmFindPDUBuf((int)need)) == NULL)
	return -oserror();
    pp->hdr.len = (int)need;
    pp->hdr.type = PDU_CREDS;
    pp->hdr.from = from;
    pp->numcreds = htonl(credcount);
    if (pmDebugOptions.context)
	for (i = 0; i < credcount; i++)
	    fprintf(stderr, "__pmSendCreds: #%d = %x\n", i, *(unsigned int*)&(credlist[i]));
    /* swab and fix bitfield order */
    for (i = 0; i < credcount; i++)
	pp->credlist[i] = __htonpmCred(credlist[i]);

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeCreds(__pmPDU *pdubuf, int *sender, int *credcount, __pmCred **credlist)
{
    creds_t	*pp;
    int		i;
    int		len;
    int		need;
    int		numcred;
    __pmCred	*list;

    pp = (creds_t *)pdubuf;
    len = pp->hdr.len;		/* ntohl() converted already in __pmGetPDU() */
    numcred = ntohl(pp->numcreds);
    if (numcred < 0 || numcred > LIMIT_CREDS)
	return PM_ERR_IPC;
    need = sizeof(creds_t) + ((numcred-1) * sizeof(__pmCred));
    if (need != len)
	return PM_ERR_IPC;

    *sender = pp->hdr.from;	/* ntohl() converted already in __pmGetPDU() */
    if ((list = (__pmCred *)malloc(sizeof(__pmCred) * numcred)) == NULL)
	return -oserror();

    /* swab and fix bitfield order */
    for (i = 0; i < numcred; i++) {
	list[i] = __ntohpmCred(pp->credlist[i]);
    }

    if (pmDebugOptions.context)
	for (i = 0; i < numcred; i++)
	    fprintf(stderr, "__pmDecodeCreds: #%d = { type=0x%x a=0x%x b=0x%x c=0x%x }\n",
		i, list[i].c_type, list[i].c_vala,
		list[i].c_valb, list[i].c_valc);

    *credlist = list;
    *credcount = numcred;

    return 0;
}
