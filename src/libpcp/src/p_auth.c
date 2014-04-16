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
#include "internal.h"
#include <ctype.h>

/*
 * PDU for per-user authentication (PDU_AUTH)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		attr;	/* PCP_ATTR code (optional, can be zero) */
    char	value[sizeof(int)];
} auth_t;

int
__pmSendAuth(int fd, int from, int attr, const char *value, int length)
{
    size_t	need;
    auth_t	*pp;
    int		i;
    int		sts;

    if (length < 0 || length >= LIMIT_AUTH_PDU)
	return PM_ERR_IPC;

    need = (sizeof(*pp) - sizeof(pp->value)) + length;
    if ((pp = (auth_t *)__pmFindPDUBuf((int)need)) == NULL)
	return -oserror();
    pp->hdr.len = (int)need;
    pp->hdr.type = PDU_AUTH;
    pp->hdr.from = from;
    pp->attr = htonl(attr);
    memcpy(&pp->value, value, length);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	char buffer[LIMIT_AUTH_PDU];
	for (i = 0; i < length; i++)
	    buffer[i] = isprint((int)value[i]) ? value[i] : '.';
	buffer[length] = buffer[LIMIT_AUTH_PDU-1] = '\0';
	if (attr)
	    fprintf(stderr, "__pmSendAuth [len=%d]: attr=%x value=\"%s\"\n",
			    length, attr, buffer);
	else
	    fprintf(stderr, "__pmSendAuth [len=%d]: payload=\"%s\"\n",
			    length, buffer);
    }
#endif

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeAuth(__pmPDU *pdubuf, int *attr, char **value, int *vlen)
{
    auth_t	*pp;
    int		i;
    int		pdulen;
    int		length;

    pp = (auth_t *)pdubuf;
    pdulen = pp->hdr.len;	/* ntohl() converted already in __pmGetPDU() */
    length = pdulen - (sizeof(*pp) - sizeof(pp->value));
    if (length < 0 || length >= LIMIT_AUTH_PDU)
	return PM_ERR_IPC;

    *attr = ntohl(pp->attr);
    *value = length ? pp->value : NULL;
    *vlen = length;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	char buffer[LIMIT_AUTH_PDU];
	for (i = 0; i < length; i++)
	    buffer[i] = isprint((int)pp->value[i]) ? pp->value[i] : '.';
	buffer[length] = buffer[LIMIT_AUTH_PDU-1] = '\0';
	if (*attr)
	    fprintf(stderr, "__pmDecodeAuth [len=%d]: attr=%x value=\"%s\"\n",
			    length, *attr, buffer);
	else
	    fprintf(stderr, "__pmDecodeAuth [len=%d]: payload=\"%s\"\n",
			    length, buffer);
    }
#endif

    return 0;
}
