/*
 * Copyright (c) 2012-2013 Red Hat.
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
 *
 * Thread-safe note
 *
 * Unlike most of the other __pmSend*() routines, there is no wrapper
 * routine in libpcp for __pmSendLogRequest() so there is no place in
 * the library to enforce serialization between the sending of the
 * PDU in __pmSendLogRequest() and reading the result PDU.
 *
 * It is assumed that the caller of __pmSendLogRequest() either manages
 * this serialization or is single-threaded, which is true for
 * the only current user of this routine, pmlc(1).
 */

#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

/*
 * PDU for general pmlogger notification (PDU_LOG_REQUEST)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		type;		/* notification type */
} notify_t;

int
__pmSendLogRequest(int fd, int type)
{
    notify_t	*pp;
    int		sts;

    if ((pp = (notify_t *)__pmFindPDUBuf(sizeof(notify_t))) == NULL)
	return -oserror();
    pp->hdr.len = sizeof(notify_t);
    pp->hdr.type = PDU_LOG_REQUEST;
    pp->hdr.from = FROM_ANON;		/* context does not matter here */
    pp->type = htonl(type);
    if (pmDebugOptions.pdu) {
	int version = __pmVersionIPC(fd);
	fprintf(stderr, "_pmSendRequest: sending PDU (type=%d, version=%d)\n",
		pp->type, version==UNKNOWN_VERSION? LOG_PDU_VERSION : version);
    }

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeLogRequest(const __pmPDU *pdubuf, int *type)
{
    const notify_t	*pp;
    const char		*pduend;

    pp = (const notify_t *)pdubuf;
    pduend = (const char *)pdubuf + pp->hdr.len;

    if (pduend - (char*)pp < sizeof(notify_t))
	return PM_ERR_IPC;

    *type = ntohl(pp->type);
    if (pmDebugOptions.pdu) {
	int version = __pmLastVersionIPC();
	fprintf(stderr, "__pmDecodeLogRequest: got PDU (type=%d, version=%d)\n",
		*type, version==UNKNOWN_VERSION? LOG_PDU_VERSION : version);
    }
    return 0;
}
