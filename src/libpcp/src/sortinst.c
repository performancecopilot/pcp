/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2022 Red Hat.
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
#include "libpcp.h"

static int
diffinsts(const void *a, const void *b)
{
    pmValue	*ap = (pmValue *)a;
    pmValue	*bp = (pmValue *)b;

    return ap->inst - bp->inst;
}

static void
sortinsts(int numpmid, pmValueSet **vset)
{
    int		i;

    for (i = 0; i < numpmid; i++) {
	if (vset[i]->numval > 1)
	    qsort(vset[i]->vlist, vset[i]->numval, sizeof(pmValue), diffinsts);
    }
}

void
__pmSortInstances(__pmResult *rp)
{
    sortinsts(rp->numpmid, &rp->vset[0]);
}

void
pmSortInstances_v2(pmResult_v2 *rp)
{
    sortinsts(rp->numpmid, &rp->vset[0]);
}

void
pmSortInstances(pmResult *rp)
{
    sortinsts(rp->numpmid, &rp->vset[0]);
}
