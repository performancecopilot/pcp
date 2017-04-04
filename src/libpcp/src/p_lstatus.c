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
 * routine in libpcp for __pmSendLogStatus() so there is no place in
 * the library to enforce serialization between the receiving the
 * LOG_REQUEST_STATUS PDU and calling __pmSendLogStatus().
 *
 * It is assumed that the caller of __pmSendLogStatus() either manages
 * this serialization or is single-threaded, which is true for
 * the only current user of this routine, pmlogger(1).
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "internal.h"

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
    int		sts;

    if ((pp = (logstatus_t *)__pmFindPDUBuf(sizeof(logstatus_t))) == NULL)
	return -oserror();
    pp->hdr.len = sizeof(logstatus_t);
    pp->hdr.type = PDU_LOG_STATUS;
    pp->hdr.from = FROM_ANON;		/* context does not matter here */
    memcpy(&pp->status, status, sizeof(__pmLoggerStatus));
    memset(&pp->pad, '~', sizeof(pp->pad));  /* initialize padding */

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
	int version = __pmVersionIPC(fd);
	fprintf(stderr, "__pmSendLogStatus: sending PDU (toversion=%d)\n",
		version == UNKNOWN_VERSION ? LOG_PDU_VERSION : version);
    }
#endif

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeLogStatus(__pmPDU *pdubuf, __pmLoggerStatus **status)
{
    logstatus_t	*pp;
    char	*pduend;

    pp = (logstatus_t *)pdubuf;
    pduend = (char *)pdubuf + pp->hdr.len;

    if ((pduend - (char*)pp) != sizeof(logstatus_t))
	return PM_ERR_IPC;

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
	int version = __pmLastVersionIPC();
	fprintf(stderr, "__pmDecodeLogStatus: got PDU (fromversion=%d)\n",
		version == UNKNOWN_VERSION ? LOG_PDU_VERSION : version);
    }
#endif
    return 0;
}
