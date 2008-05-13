/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

int
__pmStuffValue(const pmAtomValue *avp, int aggr_len, pmValue *vp, int type)
{
    void	*src;
    size_t	need, body;

    switch (type) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	    vp->value.lval = avp->ul;
	    return PM_VAL_INSITU;

	case PM_TYPE_FLOAT:
	    body = sizeof(float);
	    src  = (void *)&avp->f;
	    break;

	case PM_TYPE_64:
	case PM_TYPE_U64:
	case PM_TYPE_DOUBLE:
	    body = sizeof(__int64_t);
	    src  = (void *)&avp->ull;
	    break;

	case PM_TYPE_AGGREGATE:
	    body = aggr_len;
	    src  = avp->vp;
	    break;
	    
	case PM_TYPE_STRING:
	    body = strlen(avp->cp) + 1;
	    src  = (void *)avp->cp;
	    break;

	case PM_TYPE_AGGREGATE_STATIC:
	    vp->value.pval = (pmValueBlock *)avp->vp;
	    return PM_VAL_SPTR;

	default:
	    return PM_ERR_GENERIC;
    }
    need = body + PM_VAL_HDR_SIZE;
    if (body == sizeof(__int64_t)) {
        vp->value.pval = (pmValueBlock *)__pmPoolAlloc(need);
    } else {
        vp->value.pval = (pmValueBlock *)malloc( 
		(need < sizeof(pmValueBlock)) ? sizeof(pmValueBlock) : need);
    }
    if (vp->value.pval == NULL)
	return -errno;
    vp->value.pval->vlen = (int)need;
    vp->value.pval->vtype = type;
    memcpy((void *)vp->value.pval->vbuf, (void *)src, body);
    return PM_VAL_DPTR;
}
