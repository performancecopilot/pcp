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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
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

    pp = (ack_t *)pdubuf;
    *data = ntohl(pp->data);
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_PDU)
	fprintf(stderr, "__pmtracedecodeack -> data=%d\n",
		(int)*data);
#endif
    return 0;
}
