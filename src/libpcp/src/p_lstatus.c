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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"

extern int      errno;

/*
 * PDU for logger status information transfer (PDU_LOG_STATUS)
 */
typedef struct {
    __pmPDUHdr		hdr;
    int                 pad;            /* force status to be double word aligned */
    __pmLoggerStatus	status;
} logstatus_t;

int
__pmSendLogStatus(int fd, __pmLoggerStatus *status)
{
    logstatus_t	*pp;

    if ((pp = (logstatus_t *)__pmFindPDUBuf(sizeof(logstatus_t))) == NULL)
	return -errno;
    pp->hdr.len = sizeof(logstatus_t);
    pp->hdr.type = PDU_LOG_STATUS;
    memcpy(&pp->status, status, sizeof(__pmLoggerStatus));

    /* Conditional convertion from host to network byteorder HAVE to be
     * unconditional if one cares about endianess compatibiltity at all!
     */
    pp->status.ls_state = htonl(pp->status.ls_state);
    pp->status.ls_vol = htonl(pp->status.ls_vol);
    __htonll((char *)&pp->status.ls_size);
    pp->status.ls_start.tv_sec = htonl(pp->status.ls_start.tv_sec);
    pp->status.ls_start.tv_usec = htonl(pp->status.ls_start.tv_usec);
    pp->status.ls_last.tv_sec = htonl(pp->status.ls_last.tv_sec);
    pp->status.ls_last.tv_usec = htonl(pp->status.ls_last.tv_usec);
    pp->status.ls_timenow.tv_sec = htonl(pp->status.ls_timenow.tv_sec);
    pp->status.ls_timenow.tv_usec = htonl(pp->status.ls_timenow.tv_usec);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	__pmIPC	*ipc;
	__pmFdLookupIPC(fd, &ipc);
	fprintf(stderr, "__pmSendLogStatus: sending PDU (toversion=%d)\n",
		(ipc)?(ipc->version):(LOG_PDU_VERSION));
    }
#endif
    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeLogStatus(__pmPDU *pdubuf, __pmLoggerStatus **status)
{
    logstatus_t	*pp;

    pp = (logstatus_t *)pdubuf;

    /* Conditional convertion from host to network byteorder HAVE to be
     * unconditional if one cares about endianess compatibiltity at all!
     */
    pp->status.ls_state = ntohl(pp->status.ls_state);
    pp->status.ls_vol = ntohl(pp->status.ls_vol);
    __ntohll((char *)&pp->status.ls_size);
    pp->status.ls_start.tv_sec = ntohl(pp->status.ls_start.tv_sec);
    pp->status.ls_start.tv_usec = ntohl(pp->status.ls_start.tv_usec);
    pp->status.ls_last.tv_sec = ntohl(pp->status.ls_last.tv_sec);
    pp->status.ls_last.tv_usec = ntohl(pp->status.ls_last.tv_usec);
    pp->status.ls_timenow.tv_sec = ntohl(pp->status.ls_timenow.tv_sec);
    pp->status.ls_timenow.tv_usec = ntohl(pp->status.ls_timenow.tv_usec);

    *status = &pp->status;
    __pmPinPDUBuf(pdubuf);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	__pmIPC	*ipc;
	__pmLookupIPC(&ipc);
	fprintf(stderr, "__pmDecodeLogStatus: got PDU (fromversion=%d)\n",
		(ipc)?(ipc->version):(LOG_PDU_VERSION));
    }
#endif
    return 0;
}
