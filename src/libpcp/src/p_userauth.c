/*
 * Copyright (c) 2013 Red Hat.
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
#include <ctype.h>

#define LIMIT_USER_AUTH	2048

/*
 * PDU for per-user authentication (PDU_USER_AUTH)
 */
typedef struct {
    __pmPDUHdr	hdr;
    char	payload[sizeof(int)];
} user_auth_t;

int
__pmSendUserAuth(int fd, int from, int length, const char *payload)
{
    size_t	need;
    user_auth_t	*pp;
    int		i;
    int		sts;

    if (length < 0 || length >= LIMIT_USER_AUTH)
	return PM_ERR_IPC;

    need = (sizeof(*pp) - sizeof(pp->payload)) + length;
    if ((pp = (user_auth_t *)__pmFindPDUBuf((int)need)) == NULL)
	return -oserror();
    pp->hdr.len = (int)need;
    pp->hdr.type = PDU_USER_AUTH;
    pp->hdr.from = from;
    memcpy(&pp->payload, payload, length);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	char buffer[LIMIT_USER_AUTH] = { 0 };
	for (i = 0; i < length; i++)
	    buffer[i] = isprint(payload[i]) ? payload[i] : '.';
	fprintf(stderr, "__pmSendUserAuth [len=%d]: \"%s\"\n", length, buffer);
    }
#endif

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeUserAuth(__pmPDU *pdubuf, int *paylen, char **payload)
{
    user_auth_t	*pp;
    int		i;
    int		pdulen;
    int		length;

    pp = (user_auth_t *)pdubuf;
    pdulen = pp->hdr.len;	/* ntohl() converted already in __pmGetPDU() */
    length = pdulen - (sizeof(*pp) - sizeof(pp->payload));
    if (length < 0 || length >= LIMIT_USER_AUTH)
	return PM_ERR_IPC;

    *paylen = length;
    *payload = length ? pp->payload : NULL;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	char buffer[LIMIT_USER_AUTH] = { 0 };
	for (i = 0; i < length; i++)
	    buffer[i] = isprint(pp->payload[i]) ? pp->payload[i] : '.';
	fprintf(stderr, "__pmDecodeUserAuth [len=%d]: \"%s\"\n", length, buffer);
    }
#endif

    return 0;
}
