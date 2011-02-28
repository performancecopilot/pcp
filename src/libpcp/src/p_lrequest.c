/*
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
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"

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

    if ((pp = (notify_t *)__pmFindPDUBuf(sizeof(notify_t))) == NULL)
	return -oserror();
    pp->hdr.len = sizeof(notify_t);
    pp->hdr.type = PDU_LOG_REQUEST;
    pp->hdr.from = FROM_ANON;		/* context does not matter here */
    pp->type = htonl(type);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	int version = __pmVersionIPC(fd);
	fprintf(stderr, "_pmSendRequest: sending PDU (type=%d, version=%d)\n",
		pp->type, version==UNKNOWN_VERSION? LOG_PDU_VERSION : version);
    }
#endif
    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeLogRequest(const __pmPDU *pdubuf, int *type)
{
    const notify_t	*pp;

    pp = (const notify_t *)pdubuf;
    *type = ntohl(pp->type);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	int version = __pmLastVersionIPC();
	fprintf(stderr, "__pmDecodeLogRequest: got PDU (type=%d, version=%d)\n",
		*type, version==UNKNOWN_VERSION? LOG_PDU_VERSION : version);
    }
#endif
    return 0;
}
