/*
 * Copyright (c) 1997-2000 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "trace.h"
#include "trace_dev.h"


/*
 * PDU for general receive acknowledgement (TRACE_PDU_ACK)
 */
typedef struct {
    __pmTracePDUHdr	hdr;
    __int32_t		data;	/* ack for specific PDU type / error code */
} ack_t;

int
__pmtracesendack(int fd, __int32_t data)
{
    ack_t	*pp;

    if (__pmstate & PMTRACE_STATE_NOAGENT) {
	fprintf(stderr, "__pmtracesendack: sending acka (skipped)\n");
	return 0;
    }

    if ((pp = (ack_t *)__pmtracefindPDUbuf(sizeof(ack_t))) == NULL)
	return -oserror();
    pp->hdr.len = sizeof(ack_t);
    pp->hdr.type = TRACE_PDU_ACK;
    pp->data = htonl(data);
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_PDU)
	fprintf(stderr, "__pmtracesendack(data=%d)\n",
		(int)pp->data);
#endif
    return __pmtracexmitPDU(fd, (__pmTracePDU *)pp);
}

int
__pmtracedecodeack(__pmTracePDU *pdubuf, __int32_t *data)
{
    ack_t	*pp;
    char	*pduend;

    pp = (ack_t *)pdubuf;
    pduend = (char *)pdubuf + pp->hdr.len;

    if (pduend - (char*)pp != sizeof(ack_t))
	return PMTRACE_ERR_IPC;

    *data = ntohl(pp->data);
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_PDU)
	fprintf(stderr, "__pmtracedecodeack -> data=%d\n",
		(int)*data);
#endif
    return 0;
}
