/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
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

int
__pmControlLog(int fd, const pmResult *request, int control, int state, int delta, pmResult **status)
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
	/* get the reply */
	n = __pmGetPDU(fd, ANY_SIZE, __pmLoggerTimeout(), &pb);
	if (n == PDU_RESULT) {
	    n = __pmDecodeResult(pb, status);
	}
	else if (n == PDU_ERROR)
	    __pmDecodeError(pb, &n);
	else if (n != PM_ERR_TIMEOUT)
	    n = PM_ERR_IPC; /* unknown reply type */
    }

    return n;
}
