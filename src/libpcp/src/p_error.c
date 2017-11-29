/*
 * Copyright (c) 2012-2013 Red Hat.
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
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#include <ctype.h>

/*
 * Old V1 error codes are only used in 2 places now:
 * 1) embedded in pmResults of V1 archives, and
 * 2) as part of the client/pmcd connection challenge where all versions
 *    if pmcd return the status as a V1 error code as a legacy of
 *    migration from V1 to V2 protocols that we're stuck with (not
 *    really an issue, as the error code is normally 0)
 *
 * These macros were removed from the more public pmapi.h and impl.h
 * headers in PCP 3.6
 */
#define PM_ERR_BASE1 1000
#define PM_ERR_V1(e) (e)+PM_ERR_BASE2-PM_ERR_BASE1
#define XLATE_ERR_1TO2(e) \
	((e) <= -PM_ERR_BASE1 ? (e)+PM_ERR_BASE1-PM_ERR_BASE2 : (e))
#define XLATE_ERR_2TO1(e) \
	((e) <= -PM_ERR_BASE2 ? PM_ERR_V1(e) : (e))

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
__pmSendError(int fd, int from, int code)
{
    p_error_t	*pp;
    int		sts;

    if ((pp = (p_error_t *)__pmFindPDUBuf(sizeof(p_error_t))) == NULL)
	return -oserror();
    pp->hdr.len = sizeof(p_error_t);
    pp->hdr.type = PDU_ERROR;
    pp->hdr.from = from;

    pp->code = code;

    if (pmDebugOptions.context)
	fprintf(stderr,
		"__pmSendError: sending error PDU (code=%d, toversion=%d)\n",
		pp->code, __pmVersionIPC(fd));

    pp->code = htonl(pp->code);

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmSendXtendError(int fd, int from, int code, int datum)
{
    x_error_t	*pp;
    int		sts;

    if ((pp = (x_error_t *)__pmFindPDUBuf(sizeof(x_error_t))) == NULL)
	return -oserror();
    pp->hdr.len = sizeof(x_error_t);
    pp->hdr.type = PDU_ERROR;
    pp->hdr.from = from;

    /*
     * It is ALWAYS a PCP 1.x error code here ... this was required
     * to support migration from the V1 to V2 protocols when a V2 pmcd
     * (who is the sole user of this routine) supported connections
     * from both V1 and V2 PMAPI clients ... for the same reason we
     * cannot retire this translation, even when the V1 protocols are
     * no longer supported in all other IPC cases.
     *
     * For most common cases, code is 0 so it makes no difference.
     */
    pp->code = htonl(XLATE_ERR_2TO1(code));

    pp->datum = datum; /* NOTE: caller must swab this */

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeError(__pmPDU *pdubuf, int *code)
{
    p_error_t	*pp;
    int		sts;

    pp = (p_error_t *)pdubuf;
    if (pp->hdr.len != sizeof(p_error_t) && pp->hdr.len != sizeof(x_error_t)) {
	sts = *code = PM_ERR_IPC;
    } else {
	*code = ntohl(pp->code);
	sts = 0;
    }
    if (pmDebugOptions.context)
	fprintf(stderr,
		"__pmDecodeError: got error PDU (code=%d, fromversion=%d)\n",
		*code, __pmLastVersionIPC());
    return sts;
}

int
__pmDecodeXtendError(__pmPDU *pdubuf, int *code, int *datum)
{
    x_error_t	*pp = (x_error_t *)pdubuf;
    int		sts;

    if (pp->hdr.len != sizeof(p_error_t) && pp->hdr.len != sizeof(x_error_t)) {
	*code = PM_ERR_IPC;
    } else {
	/*
	 * It is ALWAYS a PCP 1.x error code here ... see note above
	 * in __pmSendXtendError()
	 */
	*code = XLATE_ERR_1TO2((int)ntohl(pp->code));
    }
    if (pp->hdr.len == sizeof(x_error_t)) {
	/* really version 2 extended error PDU */
	sts = PDU_VERSION2;
	*datum = pp->datum; /* NOTE: caller must swab this */
    }
    else {
	sts = PM_ERR_IPC;
    }
    if (pmDebugOptions.context)
	fprintf(stderr, "__pmDecodeXtendError: "
		"got error PDU (code=%d, datum=%d, version=%d)\n",
		*code, *datum, sts);

    return sts;
}
