/*
 * Copyright (c) 2013-2014 Red Hat.
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
#include "libpcp.h"
#include "internal.h"
#include <ctype.h>

/*
 * PDU for connection attributes (e.g. authentication, containers)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		attr;	/* PCP_ATTR code (optional, can be zero) */
    char	value[sizeof(int)];
} attr_t;

int
__pmSendAttr(int fd, int from, int attr, const char *value, int length)
{
    size_t	need;
    attr_t	*pp;
    int		i;
    int		sts;

    if (length < 0 || length >= LIMIT_ATTR_PDU)
	return PM_ERR_IPC;

    need = (sizeof(*pp) - sizeof(pp->value)) + length;
    if ((pp = (attr_t *)__pmFindPDUBuf((int)need)) == NULL)
	return -oserror();
    pp->hdr.len = (int)need;
    pp->hdr.type = PDU_ATTR;
    pp->hdr.from = from;
    pp->attr = htonl(attr);
    memcpy(&pp->value, value, length);

    if (pmDebugOptions.attr) {
	char buffer[LIMIT_ATTR_PDU];
	for (i = 0; i < length; i++)
	    buffer[i] = isprint((int)value[i]) ? value[i] : '.';
	buffer[length] = buffer[LIMIT_ATTR_PDU-1] = '\0';
	if (attr)
	    fprintf(stderr, "__pmSendAttr [len=%d]: attr=0x%x value=\"%s\"\n",
			    length, attr, buffer);
	else
	    fprintf(stderr, "__pmSendAttr [len=%d]: payload=\"%s\"\n",
			    length, buffer);
    }

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeAttr(__pmPDU *pdubuf, int *attr, char **value, int *vlen)
{
    attr_t	*pp;
    int		i;
    int		pdulen;
    int		length;

    pp = (attr_t *)pdubuf;
    pdulen = pp->hdr.len;	/* ntohl() converted already in __pmGetPDU() */
    length = pdulen - (sizeof(*pp) - sizeof(pp->value));
    if (length < 0 || length >= LIMIT_ATTR_PDU)
	return PM_ERR_IPC;

    *attr = ntohl(pp->attr);
    *value = length ? pp->value : NULL;
    *vlen = length;

    if (pmDebugOptions.attr) {
	char buffer[LIMIT_ATTR_PDU];
	for (i = 0; i < length; i++)
	    buffer[i] = isprint((int)pp->value[i]) ? pp->value[i] : '.';
	buffer[length] = buffer[LIMIT_ATTR_PDU-1] = '\0';
	if (*attr)
	    fprintf(stderr, "__pmDecodeAttr [len=%d]: attr=0x%x value=\"%s\"\n",
			    length, *attr, buffer);
	else
	    fprintf(stderr, "__pmDecodeAttr [len=%d]: payload=\"%s\"\n",
			    length, buffer);
    }

    return 0;
}

/*
 * Backward compatible interface names, from when this PDU was
 * only ever used for authentication.  Nowadays its being used
 * for other tuples too though.
 */
int
__pmSendAuth(int fd, int from, int attr, const char *value, int length)
{
    if (length >= LIMIT_AUTH_PDU)
	return PM_ERR_IPC;
    return __pmSendAttr(fd, from, attr, value, length);
}

int
__pmDecodeAuth(__pmPDU *pdubuf, int *attr, char **value, int *vlen)
{
    int sts;

    if ((sts = __pmDecodeAttr(pdubuf, attr, value, vlen)) < 0)
	return sts;
    if (*vlen >= LIMIT_AUTH_PDU)
	return PM_ERR_IPC;
    return sts;
}
