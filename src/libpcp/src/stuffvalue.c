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
 */

#include "pmapi.h"
#include "impl.h"

int
__pmStuffValue(const pmAtomValue *avp, pmValue *vp, int type)
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
	    /*
	     * vbp field of pmAtomValue points to a dynamically allocated
	     * pmValueBlock ... the vlen and vtype fields MUST have been
	     * already set up.
	     * A new pmValueBlock header will be allocated below, so adjust
	     * the length here (PM_VAL_HDR_SIZE will be added back later).
	     */
	    body = avp->vbp->vlen - PM_VAL_HDR_SIZE;
	    src  = avp->vbp->vbuf;
	    break;
	    
	case PM_TYPE_STRING:
	    body = strlen(avp->cp) + 1;
	    src  = (void *)avp->cp;
	    break;

	case PM_TYPE_AGGREGATE_STATIC:
	case PM_TYPE_EVENT:
	case PM_TYPE_HIGHRES_EVENT:
	    /*
	     * vbp field of pmAtomValue points to a statically allocated
	     * pmValueBlock ... the vlen and vtype fields MUST have been
	     * already set up and are not modified here
	     *
	     * DO NOT make a copy of the value in this case
	     */
	    vp->value.pval = avp->vbp;
	    return PM_VAL_SPTR;

	default:
	    return PM_ERR_TYPE;
    }
    need = body + PM_VAL_HDR_SIZE;
    vp->value.pval = (pmValueBlock *)malloc( 
	    (need < sizeof(pmValueBlock)) ? sizeof(pmValueBlock) : need);
    if (vp->value.pval == NULL)
	return -oserror();
    vp->value.pval->vlen = (int)need;
    vp->value.pval->vtype = type;
    memcpy((void *)vp->value.pval->vbuf, (void *)src, body);
    return PM_VAL_DPTR;
}
