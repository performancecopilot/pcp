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

#include <stdlib.h>
#include "pmapi.h"

static int
comp(const void *a, const void *b)
{
    pmValue	*ap = (pmValue *)a;
    pmValue	*bp = (pmValue *)b;

    return ap->inst - bp->inst;
}

void
pmSortInstances(pmResult *rp)
{
    int		i;

    for (i = 0; i < rp->numpmid; i++) {
	if (rp->vset[i]->numval > 1) {
	    qsort(rp->vset[i]->vlist, rp->vset[i]->numval, sizeof(pmValue), comp);
	}
    }
}
