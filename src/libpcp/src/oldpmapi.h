/*
 * Copyright (c) 1997,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */


#ifndef _OLDPMAPI_H
#define _OLDPMAPI_H

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

#define PDU_VERSION1	1

#endif /* _OLDPMAPI_H */
