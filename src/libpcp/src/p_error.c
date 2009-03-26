/*
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <ctype.h>

/*
 * PDU for general error reporting (PDU_ERROR)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		code;		/* error code */
} p_error_t;

/*
 * and the extended variant, with a second datum word
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		code;		/* error code */
    int		datum;		/* additional information */
} x_error_t;

int
__pmSendError(int fd, int mode, int code)
{
    p_error_t	*pp;

    if (mode == PDU_ASCII)
	return PM_ERR_NOASCII;
    if ((pp = (p_error_t *)__pmFindPDUBuf(sizeof(p_error_t))) == NULL)
	return -errno;
    pp->hdr.len = sizeof(p_error_t);
    pp->hdr.type = PDU_ERROR;

    if (__pmVersionIPC(fd) == PDU_VERSION1)
	pp->code = XLATE_ERR_2TO1(code);
    else
	pp->code = code;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr,
		"__pmSendError: sending error PDU (code=%d, toversion=%d)\n",
		pp->code, __pmVersionIPC(fd));
#endif

    pp->code = htonl(pp->code);

    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmSendXtendError(int fd, int mode, int code, int datum)
{
    x_error_t	*pp;

    if (mode == PDU_ASCII)
	return PM_ERR_NOASCII;
    if ((pp = (x_error_t *)__pmFindPDUBuf(sizeof(x_error_t))) == NULL)
	return -errno;
    pp->hdr.len = sizeof(x_error_t);
    pp->hdr.type = PDU_ERROR;

    pp->code = htonl(XLATE_ERR_2TO1(code));
    pp->datum = datum; /* NOTE: caller must swab this */

    return __pmXmitPDU(fd, (__pmPDU *)pp);
}

int
__pmDecodeError(__pmPDU *pdubuf, int mode, int *code)
{
    p_error_t	*pp;

    if (mode == PDU_ASCII)
	return PM_ERR_NOASCII;
    pp = (p_error_t *)pdubuf;
    if (__pmLastVersionIPC() == PDU_VERSION1)
	*code = XLATE_ERR_1TO2((int)ntohl(pp->code));
    else
	*code = ntohl(pp->code);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr,
		"__pmDecodeError: got error PDU (code=%d, fromversion=%d)\n",
		*code, __pmLastVersionIPC());
#endif
    return 0;
}

int
__pmDecodeXtendError(__pmPDU *pdubuf, int mode, int *code, int *datum)
{
    x_error_t	*pp = (x_error_t *)pdubuf;
    int		sts;

    if (mode == PDU_ASCII)
	return PM_ERR_NOASCII;
    /*
     * it is ALWAYS a PCP 1.x error code here
     */
    *code = XLATE_ERR_1TO2((int)ntohl(pp->code));

    if (pp->hdr.len == sizeof(x_error_t)) {
	/* really version 2 extended error PDU */
	sts = PDU_VERSION2;
	*datum = pp->datum; /* NOTE: caller must swab this */
    }
    else {
	/* assume a vanilla 1.x error PDU ... has same PDU type */
	sts = PDU_VERSION1;
	*datum = 0;
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmDecodeXtendError: "
		"got error PDU (code=%d, datum=%d, version=%d)\n",
		*code, *datum, sts);
#endif

    return sts;
}
