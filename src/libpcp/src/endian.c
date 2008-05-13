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

/*
 * Bit field typedefs for endian translations to support little endian
 * hosts.
 *
 * For a typedef __X_foo, the big endian version will be "foo" or
 * The only structures that appear here are ones that
 * (a) may be encoded within a PDU, and/or
 * (b) may appear in a PCP archive
 */

#include "pmapi.h"
#include "impl.h"

#ifndef __htonpmUnits
pmUnits
__htonpmUnits(pmUnits units)
{
    unsigned int	x;

    x = htonl(*(unsigned int *)&units);
    units = *(pmUnits *)&x;

    return units;
}
#endif

#ifndef __ntohpmUnits
pmUnits
__ntohpmUnits(pmUnits units)
{
    unsigned int	x;

    x = ntohl(*(unsigned int *)&units);
    units = *(pmUnits *)&x;

    return units;
}
#endif

#ifndef __htonpmValueBlock
void
__htonpmValueBlock(pmValueBlock * const vb)
{
    unsigned int	*ip = (unsigned int *) vb;

    if (vb->vtype == PM_TYPE_U64 || vb->vtype == PM_TYPE_64)
	__htonll(vb->vbuf);
    else if (vb->vtype == PM_TYPE_DOUBLE)
	__htond(vb->vbuf);
    else if (vb->vtype == PM_TYPE_FLOAT)
	__htonf(vb->vbuf);

    *ip = htonl(*ip);
}
#endif

#ifndef __ntohpmValueBlock
void
__ntohpmValueBlock(pmValueBlock * const vb)
{
    unsigned int	* tp = (unsigned int *) vb;

    /* Swab the first word, which contain vtype and vlen */
    *tp = ntohl(*tp);

    switch (vb->vtype) {
    case PM_TYPE_U64:
    case PM_TYPE_64:
	__ntohll(vb->vbuf);
	break;

    case PM_TYPE_DOUBLE:
	__ntohd(vb->vbuf);
	break;

    case PM_TYPE_FLOAT:
	__ntohf(vb->vbuf);
	break;
    }
}
#endif

#ifndef __htonpmPDUInfo
__pmPDUInfo
__htonpmPDUInfo(__pmPDUInfo info)
{
    unsigned int	x;

    x = htonl(*(unsigned int *)&info);
    info = *(__pmPDUInfo *)&x;

    return info;
}
#endif

#ifndef __ntohpmPDUInfo
__pmPDUInfo
__ntohpmPDUInfo(__pmPDUInfo info)
{
    unsigned int	x;

    x = ntohl(*(unsigned int *)&info);
    info = *(__pmPDUInfo *)&x;

    return info;
}
#endif

#ifndef __htonpmCred
__pmCred
__htonpmCred(__pmCred cred)
{
    unsigned int	x;

    x = htonl(*(unsigned int *)&cred);
    cred = *(__pmCred *)&x;

    return cred;
}
#endif

#ifndef __ntohpmCred
__pmCred
__ntohpmCred(__pmCred cred)
{
    unsigned int	x;

    x = ntohl(*(unsigned int *)&cred);
    cred = *(__pmCred *)&x;

    return cred;
}
#endif


#ifndef __htonf
void
__htonf(char *p)
{
    char 	c;
    int		i;

    for (i = 0; i < 2; i++) {
	c = p[i];
	p[i] = p[3-i];
	p[3-i] = c;
    }
}
#endif

#ifndef __htonll
void
__htonll(char *p)
{
    char 	c;
    int		i;

    for (i = 0; i < 4; i++) {
	c = p[i];
	p[i] = p[7-i];
	p[7-i] = c;
    }
}
#endif
