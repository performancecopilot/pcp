/*
 * metriclist.c
 *
 * Copyright (c) 1997,2005 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2022 Red Hat.
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
#include "libpcp.h"
#include "logger.h"

rlist_t *
mk_rlist_t(void)
{
    rlist_t	*rlist;
    if ((rlist = (rlist_t *)malloc(sizeof(rlist_t))) == NULL) {
	fprintf(stderr, "%s: Error: cannot malloc space in \"mk_rlist_t\"\n",
		pmGetProgname());
	exit(1);
    }
    rlist->res = NULL;
    rlist->next = NULL;
    return(rlist);
}


/*
 * insert rlist element in rlist list
 */
void
insertrlist(rlist_t **rlist, rlist_t *elm)
{
    rlist_t	*curr;
    rlist_t	*prev;

    if (elm == NULL)
	return;

    elm->next = NULL;

    if (*rlist == NULL) {
	*rlist = elm;
	return;
    }

    if (elm->res->timestamp.sec < (*rlist)->res->timestamp.sec ||
	(elm->res->timestamp.sec == (*rlist)->res->timestamp.sec &&
	elm->res->timestamp.nsec <= (*rlist)->res->timestamp.nsec)) {
	    curr = *rlist;
	    *rlist = elm;
	    (*rlist)->next = curr;
	    return;
    }

    curr = (*rlist)->next;
    prev = *rlist;

    while (curr != NULL) {
	if (elm->res->timestamp.sec < curr->res->timestamp.sec ||
	    (elm->res->timestamp.sec == curr->res->timestamp.sec &&
	    elm->res->timestamp.nsec <= curr->res->timestamp.nsec)) {
		break;
	}
	prev = curr;
	curr = prev->next;
    }

    prev->next = elm;
    elm->next = curr;
}


/*
 * insert __pmResult in rlist list
 */
void
insertresult(rlist_t **rlist, __pmResult *result)
{
    rlist_t	*elm;

    elm = mk_rlist_t();
    elm->res = result;
    elm->next = NULL;

    insertrlist (rlist, elm);
}

/*
 * Find out whether the metrics in _result are in the metric list ml
 * and not in the skip list skip_ml and optionally cherry-pick requested
 * instances
 */
__pmResult *
searchmlist(__pmResult *_Oresult)
{
    int		i;
    int		j;
    int		k;
    int		q;
    int		r;
    int		found = 0;
    int		maxinst = 0;		/* max number of instances */
    int		numpmid = 0;
    int		*ilist;
    int		*jlist = NULL;
    __pmResult	*_Nresult;
    pmValue	*vlistp = NULL;		/* temporary list of instances */
    pmValueSet	*vsetp;			/* value set pointer */

    /*
     * find out how many of the pmid's in _Oresult need to be written out
     * and build map ... three cases to deal with:
     * 0) if the ith pmid in _Oresult is in skip_ml[] then it will not
     *    be written out
     * 1) if configfile (ml != NULL), for the kth pmid that will be written
     *    out, ilist[k] points to the associated metric in _Oresult and
     *    jlist[k] points to the associated metric in ml[]
     *    (also, find out the maximum number of instances to write out)
     * 2) if no configfile (ml == NULL), for the kth pmid that will be
     *    written out, ilist[k] points to the associated metric in
     *    _Oresult (and all instances will be written out)
     */

    ilist = (int *) malloc(_Oresult->numpmid * sizeof(int));
    if (ilist == NULL)
	goto nomem;

    if (ml != NULL) {
	jlist = (int *) malloc(_Oresult->numpmid * sizeof(int));
	if (jlist == NULL)
	    goto nomem;
    }

    for (i=0; i<_Oresult->numpmid; i++) {
	vsetp = _Oresult->vset[i];

	for (j=0; j<skip_ml_numpmid; j++) {
	    if (vsetp->pmid == skip_ml[j])
		break;
	}
	if (j < skip_ml_numpmid) {
	    /* on skip_ml[], don't need this one */
	    continue;
	}

	if (ml != NULL) {
	    for (j=0; j<ml_numpmid; j++) {
		if (vsetp->pmid == ml[j].desc->pmid) {
		    /* pmid in _Oresult and in metric list */
		    ilist[numpmid] = i;	/* _Oresult index */
		    jlist[numpmid] = j;	/* ml list index */
		    numpmid++;
		    if (ml[j].numinst > maxinst)
			maxinst = ml[j].numinst;
		    break;
		}
	    }
	}
	else {
	    /*
	     * no configfile, want them all ...
	     */
	    ilist[numpmid] = i;	/* _Oresult index */
	    numpmid++;
	    if (vsetp->numval > maxinst)
		maxinst = vsetp->numval;
	}
    }

    /*  if no matches (no pmid's are ready for writing), then return */
    if (numpmid == 0) {
	free(ilist);
	if (ml != NULL) free(jlist);
	return(NULL);
    }

    /*
     * numpmid pmid matches were found (some or all pmid's are ready
     * for writing), so allocate space for new result
     */
    _Nresult = __pmAllocResult(numpmid);
    if (_Nresult == NULL)
	goto nomem;

    _Nresult->timestamp.sec = _Oresult->timestamp.sec;
    _Nresult->timestamp.nsec = _Oresult->timestamp.nsec;
    _Nresult->numpmid = numpmid;

    if (ml != NULL) {
	/*  vlistp[] is array for indices into vlist */
	if (maxinst > 0) {
	    vlistp = (pmValue *) malloc(maxinst * sizeof(pmValue));
	    if (vlistp == NULL)
		goto nomem;
	}
    }

    /*  point _Nresult at the right pmValueSet(s) */
    for (k=0; k<numpmid; k++) {
	i = ilist[k];
	if (ml != NULL)
	    j = jlist[k];

	/* start by assuming all instances are required */
	_Nresult->vset[k] = _Oresult->vset[i];

        if (ml != NULL && ml[j].numinst != -1 && vsetp->numval > 0) {
	    /*
	     * specific instances requested ... need to
	     * find which ones are in the __pmResult and
	     * vlistp[k] identfies the kth instance we will
	     * require
	     */
	    vsetp = _Nresult->vset[k];

	    found = 0;
	    /* requested instances loop ... */
	    for (q=0; q<ml[j].numinst; q++) {
		/* instances in __pmResult loop ... */
		for (r=0; r<vsetp->numval; r++) {
		    if (ml[j].instlist[q] == vsetp->vlist[r].inst) {
			vlistp[found].inst = vsetp->vlist[r].inst;
			vlistp[found].value = vsetp->vlist[r].value;
			++found;
			break;
		    }
		}
	    }
	    vsetp->numval = found;

	    /* now cherry-pick instances ... */
	    for (r=0; r<vsetp->numval; r++) {
	        vsetp->vlist[r].inst = vlistp[r].inst;
	        vsetp->vlist[r].value = vlistp[r].value;
	    }
        }
    }

    free(ilist);
    if (ml != NULL) free(jlist);
    if (vlistp != NULL) free(vlistp);

    return(_Nresult);

nomem:
    fprintf(stderr, "%s: Error: cannot malloc space in \"searchmlist\".\n",
	    pmGetProgname());
    exit(1);
}
