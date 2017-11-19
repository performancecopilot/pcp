/*
 * Copyright (c) 2013 Ken McDonell, Inc.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "libpcp.h"
#include "logcheck.h"

int
pass2(__pmContext *ctxp, char *archname)
{
    __pmHashNode	*hp;
    pmDesc		*dp;
    char		*name;
    int			sts;

    if (vflag)
	fprintf(stderr, "%s: start pass2\n", archname);

    /*
     * Integrity checks on metric metadata (pmDesc and PMNS)
     * pmid -> name in PMNS
     * type is valid
     * indom is PM_INDOM_NULL or domain(indom) == domain(metric)
     * sem is valid
     * units are valid
     */
    hp = __pmHashWalk(&ctxp->c_archctl->ac_log->l_hashpmid, PM_HASH_WALK_START);
    while (hp != NULL) {
	dp = (pmDesc *)hp->data;
	if ((sts = pmNameID(dp->pmid, &name)) < 0) {
	    /*
	     * Note: this is really a non-test because the pmid <-> name
	     *       association is guaranteed at this point since the 
	     *       pmid and some name have to have been loaded from the
	     *       .meta file in order that there is a entry in the hash
	     *       table.
	     */
	    fprintf(stderr, "%s.meta: PMID %s: no name in PMNS: %s\n",
		archname, pmIDStr(dp->pmid), pmErrStr(sts));
	    name = NULL;
	}
	if (dp->type == PM_TYPE_NOSUPPORT)
	    goto next;
	if (dp->type < PM_TYPE_32 || dp->type > PM_TYPE_HIGHRES_EVENT) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad type (%d) in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->type);
	}
	if (dp->indom != PM_INDOM_NULL &&
	    pmID_domain(dp->pmid) != pmInDom_domain(dp->indom)) {
	    fprintf(stderr, "%s.meta: %s [%s]: domain of pmid (%d) != domain of indom (%d)\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		pmID_domain(dp->pmid), pmInDom_domain(dp->indom));
	}
	if (dp->sem != PM_SEM_COUNTER && dp->sem != PM_SEM_INSTANT &&
	    dp->sem != PM_SEM_DISCRETE) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad semantics (%d) in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->sem);
	}
	/*
	 * Heuristic ... dimension should really be in the range -2,2
	 * (inclusive)
	 */
	if (dp->units.dimSpace < -2 || dp->units.dimSpace > 2) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad dimension (%d) for Space in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.dimSpace);
	}
	if (dp->units.dimTime < -2 || dp->units.dimTime > 2) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad dimension (%d) for Time in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.dimTime);
	}
	if (dp->units.dimCount < -2 || dp->units.dimCount > 2) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad dimension (%d) for Count in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.dimCount);
	}
	/*
	 * only Space and Time have sensible upper bounds, but if dimension
	 * is 0, scale should also be 0
	 */
	if (dp->units.dimSpace == 0 && dp->units.scaleSpace != 0) {
	    fprintf(stderr, "%s.meta: %s [%s]: non-zero scale (%d) with zero dimension for Space in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.scaleSpace);
	}
	if (dp->units.scaleSpace > PM_SPACE_EBYTE) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad scale (%d) for Space in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.scaleSpace);
	}
	if (dp->units.dimTime == 0 && dp->units.scaleTime != 0) {
	    fprintf(stderr, "%s.meta: %s [%s]: non-zero scale (%d) with zero dimension for Time in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.scaleTime);
	}
	if (dp->units.scaleTime > PM_TIME_HOUR) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad scale (%d) for Time in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.scaleTime);
	}
	if (dp->units.dimCount == 0 && dp->units.scaleCount != 0) {
	    fprintf(stderr, "%s.meta: %s [%s]: non-zero scale (%d) with zero dimension for Count in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.scaleCount);
	}


next:
	if (name != NULL)
	    free(name);

	hp = __pmHashWalk(&ctxp->c_archctl->ac_log->l_hashpmid, PM_HASH_WALK_NEXT);
    }

    return 0;
}
