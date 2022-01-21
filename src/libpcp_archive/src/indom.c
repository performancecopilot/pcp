/*
 * Copyright (c) 2014-2018,2021 Red Hat.
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2022 Ken McDonell.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "archive.h"

/*
 * Sort an indom into ascending external instance identifier order.
 * Since an indom is often already sorted, and we need to sort both
 * instlist[] and namelist[], it seems a bubble sort may be most
 * efficient
 */
void
pmaSortIndom(pmInResult *irp)
{
    int		i;
    int		j;
    int		ti;
    char	*tp;
    int		nswap;

    for (i = irp->numinst-1; i >= 0; i--) {
	nswap = 0;
	for (j = 1; j <= i; j++) {
	    if (irp->instlist[j-1] > irp->instlist[j]) {
		nswap++;
		ti = irp->instlist[j-1];
		irp->instlist[j-1] = irp->instlist[j];
		irp->instlist[j] = ti;
		tp = irp->namelist[j-1];
		irp->namelist[j-1] = irp->namelist[j];
		irp->namelist[j] = tp;
	    }
	}
	if (nswap == 0)
	    break;
    }
}

/*
 * Test if two observations of the same indom are identical ...
 * we know the indoms are both sorted.
 *
 * Return value:
 * 0 => no difference
 * 1 => different
 *
 * Version 2 comparison ... can return quickly as soon as difference
 * found.
 */
int
pmaSameIndom(pmInResult *old, pmInResult *new)
{
    int		i;
    int		sts = 1;

    if (old->numinst != new->numinst)
	goto done;

    /* internal instance identifiers */
    for (i = 0; i < old->numinst; i++) {
	if (old->instlist[i] != new->instlist[i])
	    goto done;
    }

    /*
     * external instance names (only bad PMDAs, like proc) should
     * assign different names to the same instance
     */
    for (i = 0; i < old->numinst; i++) {
	if (strcmp(old->namelist[i], new->namelist[i]) != 0)
	    goto done;
    }

    sts = 0;

done:

    if (pmDebugOptions.logmeta && pmDebugOptions.desperate) {
	fprintf(stderr, "pmaSameIndom(%s) -> %s\n", pmInDomStr(old->indom),
	    sts == 0 ? "same" : "different");
    }

    return sts;
}


/*
 * Test if two observations of the same indom are identical ...
 * we know the indoms are both sorted ... and if they are different,
 * check to see if a delta indom format would be more storage
 * efficient.
 *
 * Return value:
 * 0 => no difference
 * 1 => use full indom
 * 2 => use delta indom (and populate *new_delta)
 *
 * Version 3 comparison ... no quick return is possible.
 *
 * Note on alloc() errors: report 'em and return "1", since this
 * simply falls back to the V2 scheme (more or less).
 */
int
pmaDeltaIndom(pmInResult *old, pmInResult *new, pmInResult *new_delta)
{
    int		i;
    int		j;
    int		k;
    int		sts = 1;
    int		*old_map = NULL;
    int		*new_map = NULL;
    int		add = 0;
    int		del = 0;

    /*
     * Pass 1 ... build old_map[] and new_map[] to describe what's
     * changed
     */
    old_map = (int *)calloc(old->numinst, sizeof(int));
    if (old_map == NULL) {
	pmNoMem("pmaDeltaIndom: old_map", old->numinst * sizeof(int), PM_RECOV_ERR);
	goto done;
    }
    new_map = (int *)calloc(new->numinst, sizeof(int));
    if (new_map == NULL) {
	pmNoMem("pmaDeltaIndom: new_map", new->numinst * sizeof(int), PM_RECOV_ERR);
	goto done;
    }
    for (i = 0, j = 0; i < old->numinst || j < new->numinst; ) {
	if ((i < old->numinst && j == new->numinst) ||
	    (i < old->numinst && j < new->numinst && old->instlist[i] < new->instlist[j])) {
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "[%d] del %d -> %-29.29s\n", i, old->instlist[i], old->namelist[i]);
	    del++;
	    old_map[i] = 1;
	    i++;
	}
	else if ((i == old->numinst && j < new->numinst) ||
		 (i < old->numinst && j < new->numinst && old->instlist[i] > new->instlist[j])) {
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "[%d] add %d -> %-29.29s\n", j, new->instlist[j], new->namelist[j]);
	    add++;
	    new_map[j] = 1;
	    j++;
	}
	else if (old->instlist[i] == new->instlist[j]) {
	    if (strcmp(old->namelist[i], new->namelist[j]) != 0) {
		/*
		 * Oops ... internal instance identifier is the same, but
		 * the external instance name is different (really only the
		 * proc PMDA might do this, other PMDAs are not supposed to
		 * do this) ... just fall back to full indom
		 */
		if (pmDebugOptions.appl0)
		    fprintf(stderr, "oops! same %d -> different %-29.29s ... %-29.29s\n", old->instlist[i], old->namelist[i], new->namelist[j]);
		goto done;
	    }
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "same %d -> %-29.29s ... %-29.29s\n", old->instlist[i], old->namelist[i], new->namelist[j]);
	    i++;
	    j++;
	}
	else {
	    fprintf(stderr, "pmaDeltaIndom(): botch: i=%d old->numinst=%d j=%d new->numinst=%d\n", i, old->numinst, j, new->numinst);
	    exit(1);
	}
    }

    if (add + del == 0) {
	/* no change */
	sts = 0;
	goto done;
    }

    /*
     * Now a "del" takes a bit less space than an "add", but this
     * heuristic is close enough to picking the smaller PDU encoding
     */
    if (add + del > new->numinst)
	goto done;

    /*
     * Pass 2 - committed to delta indom now, need to build new_delta ...
     */
    new_delta->indom = new->indom;
    new_delta->numinst = add + del;
    /*
     * See comments at head of function re. alloc() failures ...
     */
    new_delta->instlist = (int *)malloc(new_delta->numinst * sizeof(int));
    if (new_delta->instlist == NULL) {
	pmNoMem("pmaDeltaIndom: new instlist", new_delta->numinst * sizeof(int), PM_RECOV_ERR);
	goto done;
    }
    new_delta->namelist = (char **)malloc(new_delta->numinst * sizeof(char *));
    if (new_delta->namelist == NULL) {
	pmNoMem("pmaDeltaIndom: new namelist", new_delta->numinst * sizeof(char *), PM_RECOV_ERR);
	goto done;
    }
    /*
     * need to emit instances in sorted abs(instance number) order ...
     * the logic on the read-path in __pmLogUndeltaInDom() assumes this
     */
    for (i = j = k = 0; i < old->numinst || j < new->numinst; ) {
	if (i < old->numinst && j < new->numinst) {
	    if (!old_map[i]) {
		i++;
		continue;
	    }
	    if (!new_map[j]) {
		j++;
		continue;
	    }
	    /*
	     * both are candidates ... choose the smaller instance number
	     */
	    if (old->instlist[i] < new->instlist[j]) {
		/* delete instance in middle of indom */
		new_delta->instlist[k] = -old->instlist[i];
		new_delta->namelist[k] = NULL;
		k++;
		i++;
	    }
	    else {
		/* add instance in middle of indom */
		new_delta->instlist[k] = new->instlist[j];
		new_delta->namelist[k] = new->namelist[j];
		k++;
		j++;
	    }
	    continue;
	}
	if (i < old->numinst) {
	    if (old_map[i]) {
		/* delete from end of indom */
		new_delta->instlist[k] = -old->instlist[i];
		new_delta->namelist[k] = NULL;
		k++;
	    }
	    i++;
	}
	else {
	    /* j < new->numinst */
	    if (new_map[j]) {
		/* add to end of indom */
		new_delta->instlist[k] = new->instlist[j];
		new_delta->namelist[k] = new->namelist[j];
		k++;
	    }
	    j++;
	}
    }
    sts = 2;

done:
    if (old_map != NULL)
	free(old_map);
    if (new_map != NULL)
	free(new_map);

    if (pmDebugOptions.logmeta && pmDebugOptions.desperate) {
	fprintf(stderr, "pmaDeltaIndom(%s) -> %s\n", pmInDomStr(old->indom),
	    sts == 0 ? "same" : ( sts == 1 ? "full indom" : "delta indom" ));
    }

    return sts;
}
