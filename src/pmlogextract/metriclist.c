/*
 * metriclist.c
 *
 * Copyright (c) 1997,2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "impl.h"
#include "logger.h"

/*
 * extract the pmid in vsetp and all its instances, and put it in
 * a pmResult of its own
 */
void
extractpmid(pmValueSet *vsetp, struct timeval *timestamp, pmResult **resp)
{
    int			i;
    int			size;
    pmResult		*result;
    pmValueBlock	*vbp;		/* value block pointer */


    result = (pmResult *)malloc(sizeof(pmResult));
    if (result == NULL) {
	fprintf(stderr, "%s: Error: cannot malloc space in \"extractpmid\".\n",
		pmProgname);
	exit(1);
    }

    size = sizeof(pmValueSet) + (vsetp->numval-1) * sizeof(pmValue);
    result->vset[0] = (pmValueSet *)malloc(size);
    if (result->vset[0] == NULL) {
	fprintf(stderr, "%s: Error: cannot malloc space in \"extractpmid\".\n",
		pmProgname);
	exit(1);
    }


    result->timestamp.tv_sec = timestamp->tv_sec;
    result->timestamp.tv_usec = timestamp->tv_usec;
    result->numpmid = 1;
    result->vset[0]->pmid = vsetp->pmid;
    result->vset[0]->numval = vsetp->numval;
    result->vset[0]->valfmt = vsetp->valfmt;


    for(i=0; i<vsetp->numval; i++) {
	result->vset[0]->vlist[i].inst = vsetp->vlist[i].inst;
	if (vsetp->valfmt == PM_VAL_INSITU)
	    result->vset[0]->vlist[i].value = vsetp->vlist[i].value;
	else {
	    vbp = vsetp->vlist[i].value.pval;

	    size = (int)vbp->vlen;
	    result->vset[0]->vlist[i].value.pval = (pmValueBlock *)malloc(size);
	    if (result->vset[0]->vlist[i].value.pval == NULL) {
		fprintf(stderr,
		    "%s: Error: cannot malloc space in \"extractpmid\".\n",
			pmProgname);
		exit(1);
    	    }

	    result->vset[0]->vlist[i].value.pval->vtype = vbp->vtype;
	    result->vset[0]->vlist[i].value.pval->vlen = vbp->vlen;

	    /* in a pmValueBlock, the first byte is assigned to vtype,
	     * and the subsequent 3 bytes are assigned to vlen - that's
	     * a total of 4 bytes - the rest is used for vbuf
	     */
	    if (vbp->vlen < 4) {
		fprintf(stderr, "%s: Warning: pmValueBlock vlen (%u) is too small\n", pmProgname, vbp->vlen);
	    }
	    memcpy(result->vset[0]->vlist[i].value.pval->vbuf,
						vbp->vbuf, vbp->vlen-4);
	}
    } /*for(i)*/

    *resp = result;
}

rlist_t *
mk_rlist_t(void)
{
    rlist_t	*rlist;
    if ((rlist = (rlist_t *)malloc(sizeof(rlist_t))) == NULL) {
	fprintf(stderr, "%s: Error: cannot malloc space in \"mk_rlist_t\"\n",
		pmProgname);
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

    if (elm->res->timestamp.tv_sec < (*rlist)->res->timestamp.tv_sec ||
	(elm->res->timestamp.tv_sec == (*rlist)->res->timestamp.tv_sec &&
	elm->res->timestamp.tv_usec <= (*rlist)->res->timestamp.tv_usec)) {
	    curr = *rlist;
	    *rlist = elm;
	    (*rlist)->next = curr;
	    return;
    }

    curr = (*rlist)->next;
    prev = *rlist;

    while (curr != NULL) {
	if (elm->res->timestamp.tv_sec < curr->res->timestamp.tv_sec ||
	    (elm->res->timestamp.tv_sec == curr->res->timestamp.tv_sec &&
	    elm->res->timestamp.tv_usec <= curr->res->timestamp.tv_usec)) {
		break;
	}
	prev = curr;
	curr = prev->next;
    }

    prev->next = elm;
    elm->next = curr;
}


/*
 * insert pmResult in rlist list
 */
void
insertresult(rlist_t **rlist, pmResult *result)
{
    rlist_t	*elm;

    elm = mk_rlist_t();
    elm->res = result;
    elm->next = NULL;

    insertrlist (rlist, elm);
}

/*
 * Find out whether the metrics in _result are in the metric list ml
 */
pmResult *
searchmlist(pmResult *_Oresult)
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
    int		*jlist;
    pmResult	*_Nresult;
    pmValue	*vlistp = NULL;		/* temporary list of instances */
    pmValueSet	*vsetp;			/* value set pointer */

    ilist = (int *) malloc(_Oresult->numpmid * sizeof(int));
    if (ilist == NULL)
	goto nomem;

    jlist = (int *) malloc(_Oresult->numpmid * sizeof(int));
    if (jlist == NULL)
	goto nomem;

    /* find out how many of the pmid's in _Oresult need to be written out
     * (also, find out the maximum number of instances to write out)
     */
    numpmid = 0;
    maxinst = 0;
    for (i=0; i<_Oresult->numpmid; i++) {
	vsetp = _Oresult->vset[i];

	for (j=0; j<ml_numpmid; j++) {
	    if (vsetp->pmid == ml[j].idesc->pmid) {
		/* pmid has been found in metric list
		 */
		if (ml[j].numinst > maxinst)
		    maxinst = ml[j].numinst;

		++numpmid;
		ilist[numpmid-1] = i;	/* _Oresult index */
		jlist[numpmid-1] = j;	/* ml list index */
		break;
	    }
	}
    }


    /*  if no matches (no pmid's are ready for writing), then return
     */
    if (numpmid == 0) {
	free(ilist);
	free(jlist);
	return(NULL);
    }


    /*  `numpmid' matches were found (some or all pmid's are ready for writing),
     *	then allocate space for new result
     */
    _Nresult = (pmResult *) malloc(sizeof(pmResult) +
					(numpmid - 1) * sizeof(pmValueSet *));
    if (_Nresult == NULL)
	goto nomem;

    _Nresult->timestamp.tv_sec = _Oresult->timestamp.tv_sec;
    _Nresult->timestamp.tv_usec = _Oresult->timestamp.tv_usec;
    _Nresult->numpmid = numpmid;


    /*  make array for indeces into vlist
     */
    if (maxinst > 0) {
	vlistp = (pmValue *) malloc(maxinst * sizeof(pmValue));
	if (vlistp == NULL)
	    goto nomem;
    }


    /*  point _Nresult at the right pmValueSet(s)
     */
    for (k=0; k<numpmid; k++) {
	i = ilist[k];
	j = jlist[k];

	/* point new result at the wanted pmid
	 */
	_Nresult->vset[k] = _Oresult->vset[i];


	/* allocate the right instances
	 */
	vsetp = _Nresult->vset[k];

	found = 0;
	for (q=0; q<ml[j].numinst; q++) {
	    for (r=0; r<vsetp->numval; r++) {

		/* if id in ml is -1, chances are that we haven't seen
		 * it before ... set the instance id
		 */
		if (ml[j].instlist[q] < 0)
		    ml[j].instlist[q] = vsetp->vlist[r].inst;

		if (ml[j].instlist[q] == vsetp->vlist[r].inst) {
		    /* instance has been found
		     */
		    vlistp[found].inst = vsetp->vlist[r].inst;
		    vlistp[found].value = vsetp->vlist[r].value;
		    ++found;
		    break;
		} /*if*/
	    } /*for(r)*/
	} /*for(q)*/


	/* note: found may be <= ml[j].numinst
	 *	 further more, found may be zero ... deal with this later?
	 *		- NUMVAL
         *
         * note2: if ml[j].numinst == -1, it means we want all insts.
         *        ignore the fact that found is 0.
	 */
        if( ml[j].numinst != -1 ){
	    vsetp->numval = found;

	    for (q=0; q<vsetp->numval; q++) {
	        vsetp->vlist[q].inst = vlistp[q].inst;
	        vsetp->vlist[q].value = vlistp[q].value;
	        vlistp[q].inst = 0;
	        vlistp[q].value.lval = 0;
	    } /*for(q)*/
        } /* if */
    } /*for(k)*/

    free(ilist);
    free(jlist);
    if (maxinst > 0) free(vlistp);	/* free only if space was allocated */
    return(_Nresult);

nomem:
    fprintf(stderr, "%s: Error: cannot malloc space in \"searchmlist\".\n",
	    pmProgname);
    exit(1);
}
