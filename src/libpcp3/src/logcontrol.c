/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2022 Red Hat.
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
 * This routine is not thread-safe as there is no serialization on the
 * use of the fd between the __pmSendLogControl() and the reading of
 * the reply PDU.  It is assumed that the caller is single-threaded,
 * which is true for the only current user of this routine, pmlc(1).
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

int
__pmControlLog(int fd, const __pmResult *request, int control, int state, int delta, __pmResult **status)
{
    int         	n;
    __pmPDU     	*pb;

    if (request->numpmid < 1)
        return PM_ERR_TOOSMALL;

    /* send a PCP 2.0 log control request */
    n = __pmSendLogControl(fd, request, control, state, delta);
    if (n < 0)
	n = __pmMapErrno(n);
    else {
	int		pinpdu;
	/* get the reply */
	pinpdu = n = __pmGetPDU(fd, ANY_SIZE, __pmLoggerTimeout(), &pb);
	if (n == PDU_RESULT) {
	    n = __pmDecodeResult(pb, status);
	}
	else if (n == PDU_ERROR)
	    __pmDecodeError(pb, &n);
	else if (n != PM_ERR_TIMEOUT) {
	    if (pmDebugOptions.pdu) {
		char	strbuf[20];
		char	errmsg[PM_MAXERRMSGLEN];
		if (n < 0)
		    fprintf(stderr, "__pmControlLog: PM_ERR_IPC: expecting PDU_RESULT but__pmGetPDU returns %d (%s)\n",
			n, pmErrStr_r(n, errmsg, sizeof(errmsg)));
		else
		    fprintf(stderr, "__pmControlLog: PM_ERR_IPC: expecting PDU_RESULT but__pmGetPDU returns %d (type=%s)\n",
			n, __pmPDUTypeStr_r(n, strbuf, sizeof(strbuf)));
	    }
	    n = PM_ERR_IPC; /* unknown reply type */
	}
	if (pinpdu > 0)
	    __pmUnpinPDUBuf(pb);
    }

    return n;
}
