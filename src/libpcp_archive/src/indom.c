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

#include "pcp/pmapi.h"
#include "pcp/libpcp.h"
#include "../libpcp/src/internal.h"
#include "pcp/archive.h"

/*
 * Sort an indom into ascending external instance identifier order.
 * Since an indom is often already sorted, and we need to sort both
 * instlist[] and namelist[], it seems a bubble sort may be most
 * efficient
 */
void
pmaSortInDom(__pmLogInDom *lidp)
{
    int		i;
    int		j;
    int		ti;
    char	*tp;
    int		nswap;

    for (i = lidp->numinst-1; i >= 0; i--) {
	nswap = 0;
	for (j = 1; j <= i; j++) {
	    if (lidp->instlist[j-1] > lidp->instlist[j]) {
		nswap++;
		ti = lidp->instlist[j-1];
		lidp->instlist[j-1] = lidp->instlist[j];
		lidp->instlist[j] = ti;
		tp = lidp->namelist[j-1];
		lidp->namelist[j-1] = lidp->namelist[j];
		lidp->namelist[j] = tp;
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
 * 1 => no difference
 * 0 => different
 */
int
pmaSameInDom(__pmLogInDom *old, __pmLogInDom *new)
{
    int		i;
    int		sts = 0;

    if (old->numinst != new->numinst)
	goto done;

    /* check internal instance identifiers */
    for (i = 0; i < old->numinst; i++) {
	if (old->instlist[i] != new->instlist[i])
	    goto done;
    }

    /*
     * check external instance names:  only bad PMDAs (like proc)
     * should assign different names to the same instance
     */
    for (i = 0; i < old->numinst; i++) {
	if (strcmp(old->namelist[i], new->namelist[i]) != 0)
	    goto done;
    }

    sts = 1;

done:

    if (pmDebugOptions.indom) {
	fprintf(stderr, "pmaSameInDom(%s) -> %s\n", pmInDomStr(old->indom),
	    sts == 1 ? "same" : "different");
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
 * Only makes sense to be called for V3 archives ... an assumption we
 * cannot check here as the args don't give a way to determine the
 * associated archive version.
 *
 * Note on alloc() errors in pmaDeltaInDom():
 *     report 'em and use pmaSameInDom() ... this simply falls back
 *     to the V2 scheme (more or less).
 */
int
pmaDeltaInDom(__pmLogInDom *old, __pmLogInDom *new, __pmLogInDom *new_delta)
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
	pmNoMem("pmaDeltaInDom: old_map", old->numinst * sizeof(int), PM_RECOV_ERR);
	goto fallback;
    }
    new_map = (int *)calloc(new->numinst, sizeof(int));
    if (new_map == NULL) {
	pmNoMem("pmaDeltaInDom: new_map", new->numinst * sizeof(int), PM_RECOV_ERR);
	goto fallback;
    }
    for (i = 0, j = 0; i < old->numinst || j < new->numinst; ) {
	if ((i < old->numinst && j == new->numinst) ||
	    (i < old->numinst && j < new->numinst && old->instlist[i] < new->instlist[j])) {
	    if (pmDebugOptions.indom)
		fprintf(stderr, "[%d] del %d -> %-29.29s\n", i, old->instlist[i], old->namelist[i]);
	    del++;
	    old_map[i] = 1;
	    i++;
	}
	else if ((i == old->numinst && j < new->numinst) ||
		 (i < old->numinst && j < new->numinst && old->instlist[i] > new->instlist[j])) {
	    if (pmDebugOptions.indom)
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
		if (pmDebugOptions.indom)
		    fprintf(stderr, "oops! same %d -> different %-29.29s ... %-29.29s\n", old->instlist[i], old->namelist[i], new->namelist[j]);
		goto done;
	    }
	    if (pmDebugOptions.indom)
		fprintf(stderr, "same %d -> %-29.29s ... %-29.29s\n", old->instlist[i], old->namelist[i], new->namelist[j]);
	    i++;
	    j++;
	}
	else {
	    fprintf(stderr, "pmaDeltaInDom(): botch: i=%d old->numinst=%d j=%d new->numinst=%d\n", i, old->numinst, j, new->numinst);
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
    new_delta->next = new_delta->prior = NULL;
    new_delta->buf = NULL;
    new_delta->indom = new->indom;
    new_delta->stamp = new->stamp;
    new_delta->isdelta = 1;
    new_delta->numinst = add + del;
    new_delta->alloc = (PMLID_INSTLIST | PMLID_NAMELIST);
    /*
     * See comments at head of function re. alloc() failures ...
     */
    new_delta->instlist = (int *)malloc(new_delta->numinst * sizeof(int));
    if (new_delta->instlist == NULL) {
	pmNoMem("pmaDeltaInDom: new instlist", new_delta->numinst * sizeof(int), PM_RECOV_ERR);
	goto fallback;
    }
    new_delta->namelist = (char **)malloc(new_delta->numinst * sizeof(char *));
    if (new_delta->namelist == NULL) {
	pmNoMem("pmaDeltaInDom: new namelist", new_delta->numinst * sizeof(char *), PM_RECOV_ERR);
	free(new_delta->instlist);
	goto fallback;
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
		new_delta->instlist[k] = old->instlist[i];
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
		new_delta->instlist[k] = old->instlist[i];
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
    goto done;

fallback:
    /*
     * if we have a malloc failure, revert to the old-style (V2)
     * pmaSameInDom() method, but notice the return codes are
     * inverted
     */
    sts = 1 - pmaSameInDom(old, new);

done:
    if (old_map != NULL)
	free(old_map);
    if (new_map != NULL)
	free(new_map);

    if (pmDebugOptions.indom) {
	fprintf(stderr, "pmaDeltaInDom(%s) -> %s\n", pmInDomStr(old->indom),
	    sts == 0 ? "same" : ( sts == 1 ? "full indom" : "delta indom" ));
    }

    return sts;
}

/*
 * Given an external metadata record that contains a "delta" indom
 * (in buf), find the corresponding __pmLogInDom structure from
 * lcp->hashindom.
 *
 * This assumes the archive has also been opened with pmNewContext
 * so that all of the metadata has been loaded.
 *
 * The __pmLogInDom structure is expected to be found and the search
 * will also ensure the indom has been "undelta'd" in the process.
 */
__pmLogInDom *
pmaUndeltaInDom(__pmLogCtl *lcp, __int32_t *buf)
{
    pmInDom		indom;
    __pmTimestamp	stamp;
    __pmLogInDom	*idp;

    __pmLoadTimestamp(&buf[2], &stamp);
    indom = __ntohpmInDom(buf[5]);

    idp = __pmLogSearchInDom(lcp, indom, &stamp);

    if (idp != NULL)
	return idp;
    else {
	int		numinst;
	__int32_t	*stridx;
	char		*strbase;
	int		idx;
	int		i;
	int		j;
	fprintf(stderr, "pmaUndeltaInDom: Botch: indom %s @ ", pmInDomStr(indom));
	__pmPrintTimestamp(stderr, &stamp);
	fprintf(stderr, ": not found from __pmLogCtl\n");
	numinst = ntohl(buf[6]);
	fprintf(stderr, "InDom from archive record (numinst %d) ...\n", numinst);
	j = 7;
	stridx = &buf[j + numinst];
	strbase = (char *)&stridx[numinst];
	for (i = 0; i < numinst; i++) {
	    idx = ntohl(*stridx);
	    fprintf(stderr, "[%d] %d idx=%d", i, ntohl(buf[j]), idx);
	    if (idx >= 0)
		fprintf(stderr, " add \"%s\"\n", &strbase[idx]);
	    else
		fprintf(stderr, " del\n");
	    stridx++;
	    j++;
	}
	return NULL;
    }
}

static __pmHashCtl	hashindom;
typedef struct {
    __int32_t		*buf;
    __pmLogInDom	lid;
}
indom_ctl;

/*
 * The input indom is provided either via lidp (an already loaded 
 * indom) or rbuf (a physical indom record) for a V3 archive ...
 * AND this must be the same for ALL calls to pmaTryDeltaInDom()
 * for a given lcp.
 *
 * pmaTryDeltaInDom() checks to see if "delta" indom encoding would
 * be more efficient ... this involves maintaining a per-indom copy
 * of the last "full" indom that was seen by pmaTryDeltaInDom().
 *
 * If "delta" indom format is preferred, a new indom is returned
 * via lidpp or rbuf and the old one is free'd
 *
 * In the rbuf case we need to extract the __pmInDom from the
 * initial rbuf and this involves rewriting rbuf (at least some ntohl()
 * magic) and also because we keep the __pmInDom in a hashed cache
 * until the next time we see the same indom, pmaTryDeltaInDom() makes
 * a copy of rbuf.
 */
int
pmaTryDeltaInDom(__pmLogCtl *lcp, __int32_t **rbuf, __pmLogInDom *lidp)
{
    int			rlen;
    int			type;
    int			sts = 0;
    int			i;
    __int32_t		*tmp;
    pmInDom		indom;
    indom_ctl		this;
    indom_ctl		*last;
    __pmHashNode	*hnp;
    int			first_time = 0;

    /*
     * Need exactly ONE of rbuf and lidp to be NULL ... the real
     * input indom is provided by the other parameter.
     */
    if ((rbuf == NULL && lidp == NULL) || (rbuf != NULL && lidp != NULL))
	return -2;

    if (rbuf) {
	type = ntohl((*rbuf)[1]);
	if (type != TYPE_INDOM) {
	    /* if not V3 indom record, nothing to do */
	    return -1;
	}
	rlen = ntohl((*rbuf)[0]);

	/* make a copy of rbuf */
	if ((this.buf = (__int32_t *)malloc(rlen)) == NULL) {
	    pmNoMem("pmaTryDeltaInDom: buf copy", rlen, PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	memcpy(this.buf, *rbuf, rlen);

	indom = __ntohpmInDom(this.buf[5]);
	tmp = &this.buf[2];
	if ((sts = __pmLogLoadInDom(NULL, rlen, type, &this.lid, &tmp)) < 0) {
	    fprintf(stderr, "pmaTryDeltaInDom: Botch: __pmLogLoadInDom for indom %s: %s\n",
		pmInDomStr(indom), pmErrStr(sts));
	    exit(1);
	}
    }
    else {
	__pmLogInDom	*tmp_lidp;
	this.buf = NULL;
	indom = lidp->indom;
	if ((tmp_lidp = __pmDupLogInDom(lidp)) == NULL) {
	    fprintf(stderr, "pmaTryDeltaInDom: Botch: __pmDupLogInDom for indom %s: NULL\n",
		pmInDomStr(indom));
	    exit(1);
	}
	this.lid = *tmp_lidp;		/* struct assignment */
	this.lid.alloc &= ~PMLID_SELF;	/* don't free this */
	free(tmp_lidp);
    }

    if ((hnp = __pmHashSearch((unsigned int)indom, &hashindom)) == NULL) {
	indom_ctl	*new;
	int		lsts;
	/* first time for this indom */
	if  ((lsts = __pmHashAdd((unsigned int)indom, NULL, &hashindom)) < 0) {
	    fprintf(stderr, "pmaTryDeltaInDom: Botch: __pmHashAdd for indom %s: %s\n",
		pmInDomStr(indom), pmErrStr(lsts));
	    exit(1);
	}
	if ((hnp = __pmHashSearch((unsigned int)indom, &hashindom)) == NULL) {
	    fprintf(stderr, "pmaTryDeltaInDom: Botch: __pmHashSearch for indom %s: failed\n",
		pmInDomStr(indom));
	    exit(1);
	}
	new = (indom_ctl *)malloc(sizeof(*new));
	if (new == NULL) {
	    pmNoMem("pmaTryDeltaInDom: indom_ctl malloc", sizeof(*new), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	hnp->data = (void *)new;
	last = new;
	first_time = 1;
    }
    else {
	__pmLogInDom		delta;
	int			lsts;
	last = (indom_ctl *)hnp->data;
	lsts = pmaDeltaInDom(&last->lid, &this.lid, &delta);
	if (lsts == 2) {
	    __int32_t		*new;
	    /*
	     * "delta" indom is preferred ... rewrite away
	     */
	    if (rbuf) {
		lsts = __pmLogEncodeInDom(lcp, TYPE_INDOM_DELTA, &delta, &new);
		if (lsts < 0) {
		    fprintf(stderr, "pmaTryDeltaInDom: Botch: __pmLogEncodeInDom for indom %s: %s\n",
			pmInDomStr(indom), pmErrStr(lsts));
		    exit(1);
		}
		free(*rbuf);
		*rbuf = new;
		__pmFreeLogInDom(&delta);

		/* free old "last" record buffer */
		free(last->buf);
	    }
	    else {
		/*
		 * pmaDeltaIndom() above leaves delta.namelist[i] elements
		 * pointing into the strings of last->lid->namelist[j] ,,,
		 * we need to copy these so *lidp survives if last is
		 * free'd
		 */
		for (i = 0; i < delta.numinst; i++) {
		    if (delta.namelist[i] != NULL) {
			char	*name;
			if ((name = strdup(delta.namelist[i])) == NULL) {
			    pmNoMem("pmaTryDeltaInDom: namelist[i]", strlen(delta.namelist[i]), PM_FATAL_ERR);
			    /*NOTREACHED*/
			}
			delta.namelist[i] = name;
		    }
		}
		delta.alloc |= PMLID_NAMES;
		__pmFreeLogInDom(lidp);
		*lidp = delta;			/* struct assignment */
	    }
	    sts = 1;
	}
	else if (lsts < 0) {
	    fprintf(stderr, "pmaTryDeltaInDom: Botch: pmaDeltaInDom for indom %s: %s\n",
		pmInDomStr(indom), pmErrStr(lsts));
	    exit(1);
	}
	else {
	    /* not using "delta" indom */
	    if (rbuf) {
		/* free old "last" record buffer */
		free(last->buf);
	    }
	}
    }

    /* for next time we're called ... this -> last */
    if (!first_time)
	__pmFreeLogInDom(&last->lid);
    memcpy(last, &this, sizeof(this));

    return sts;
}
