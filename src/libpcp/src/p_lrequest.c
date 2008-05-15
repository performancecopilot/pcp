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
	return -errno;
    pp->hdr.len = sizeof(notify_t);
    pp->hdr.type = PDU_LOG_REQUEST;
    pp->type = htonl(type);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	__pmIPC	*ipc;
	__pmFdLookupIPC(fd, &ipc);
	fprintf(stderr, "_pmSendRequest: sending PDU (type=%d, version=%d)\n",
		pp->type, (ipc)?(ipc->version):(LOG_PDU_VERSION));
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
	__pmIPC	*ipc;
	__pmLookupIPC(&ipc);
	fprintf(stderr, "__pmDecodeLogRequest: got PDU (type=%d, version=%d)\n",
		*type, (ipc)?(ipc->version):(LOG_PDU_VERSION));
    }
#endif
    return 0;
}
