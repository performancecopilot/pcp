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

    if (length < 0 || length >= LIMIT_AUTH_PDU)
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
	char buffer[LIMIT_AUTH_PDU] = { 0 };
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
    if (length < 0 || length >= LIMIT_AUTH_PDU)
	return PM_ERR_IPC;

    *paylen = length;
    *payload = length ? pp->payload : NULL;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	char buffer[LIMIT_AUTH_PDU] = { 0 };
	for (i = 0; i < length; i++)
	    buffer[i] = isprint(pp->payload[i]) ? pp->payload[i] : '.';
	fprintf(stderr, "__pmDecodeUserAuth [len=%d]: \"%s\"\n", length, buffer);
    }
#endif

    return 0;
}

/*
 * PDU for authentication attributes (PDU_AUTH_ATTR)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		attr;
    char	value[sizeof(int)];
} auth_attr_t;

int
__pmSendAuthAttr(int fd, int from, int attr, int length, const char *value)
{
    size_t	need;
    auth_attr_t	*pp;
    int		i;
    int		sts;

    if (length < 0 || length >= LIMIT_AUTH_PDU + sizeof(int))
	return PM_ERR_IPC;

    need = (sizeof(*pp) - sizeof(pp->value)) + length;
    if ((pp = (auth_attr_t *)__pmFindPDUBuf((int)need)) == NULL)
	return -oserror();
    pp->hdr.len = (int)need;
    pp->hdr.type = PDU_AUTH_ATTR;
    pp->hdr.from = from;
    pp->attr = htonl(attr);
    memcpy(&pp->value, value, length);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	char buffer[LIMIT_AUTH_PDU] = { 0 };
	for (i = 0; i < length; i++)
	    buffer[i] = isprint(value[i]) ? value[i] : '.';
	fprintf(stderr, "__pmSendAuthAttr [len=%d]: attr=%x value=\"%s\"\n",
		attr, length, buffer);
    }
#endif

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeAuthAttr(__pmPDU *pdubuf, int *attr, int *vlen, char **value)
{
    auth_attr_t	*pp;
    int		i;
    int		pdulen;
    int		length;

    pp = (auth_attr_t *)pdubuf;
    pdulen = pp->hdr.len;	/* ntohl() converted already in __pmGetPDU() */
    length = pdulen - (sizeof(*pp) - sizeof(pp->value));
    if (length < 0 || length >= LIMIT_AUTH_PDU + sizeof(int))
	return PM_ERR_IPC;

    *attr = ntohl(pp->attr);
    *vlen = length;
    *value = length ? pp->value : NULL;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	char buffer[LIMIT_AUTH_PDU] = { 0 };
	for (i = 0; i < length; i++)
	    buffer[i] = isprint(pp->value[i]) ? pp->value[i] : '.';
	fprintf(stderr, "__pmDecodeAuthAttr [len=%d]: attr=%x value=\"%s\"\n",
		length, *attr, buffer);
    }
#endif

    return 0;
}
