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
 * Generic routines to provide the "optimized" pmFetch bundling
 * services ... the optimization is driven by the crude heuristics
 * weights defined in optcost below.
 *
 * Thread-safe notes
 *
 * lrand48() is not thread-safe, but we don't really care here.
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#include <assert.h>

/*
 * elements of optcost are
 *	
 *  c_pmid	cost per PMD for PMIDs in a fetch
 *  c_indom	cost per PMD for indoms in a fetch
 *  c_fetch	cost of a new fetch group (should be less than
 *		c_indomsize * c_xtrainst, else all fetches will go
 *		into a single group, unless the scope is global)
 *  c_indomsize	expected numer of instances for an indom
 *  c_xtrainst	cost of retrieving an unwanted metric inst
 *  c_scope	cost opt., 0 for incremental, 1 for global
 */

/* default costs */
static optcost_t	optcost = { 4, 1, 15, 10, 2, 0 };

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	optfetch_lock = PTHREAD_MUTEX_INITIALIZER;
#else
void			*optfetch_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == util_lock
 */
int
__pmIsOptfetchLock(void *lock)
{
    return lock == (void *)&optfetch_lock;
}
#endif

static int
addpmid(fetchctl_t *fp, pmID pmid)
{
    int		i;
    int		j;

    for (i = 0; i < fp->f_numpmid; i++) {
	if (pmid == fp->f_pmidlist[i])
	    return 0;
	if (pmid > fp->f_pmidlist[i])
	    break;
    }
    fp->f_numpmid++;
    fp->f_pmidlist = (pmID *)realloc(fp->f_pmidlist, fp->f_numpmid*sizeof(pmID));
    if (fp->f_pmidlist == NULL) {
	PM_UNLOCK(optfetch_lock);
	pmNoMem("addpmid", fp->f_numpmid*sizeof(pmID), PM_FATAL_ERR);
    }

    for (j = fp->f_numpmid-1; j > i; j--)
	fp->f_pmidlist[j] = fp->f_pmidlist[j-1];
    fp->f_pmidlist[i] = pmid;

    return 1;
}

static int
addinst(int *numinst, int **instlist, optreq_t *new)
{
    int		i;
    int		j;
    int		k;
    int		numi;
    int		*ilist;
    int		match;

    if (*numinst == 0)
	return 0;
    if (new->r_numinst == 0) {
	*numinst = 0;
	if (*instlist != NULL) {
	    free(*instlist);
	    *instlist = NULL;
	}
	return 1;
    }
    numi = *numinst;
    if (numi == -1)
	numi = 0;

    ilist = (int *)realloc(*instlist, (numi + new->r_numinst)*sizeof(int));
    if (ilist == NULL) {
	PM_UNLOCK(optfetch_lock);
	pmNoMem("addinst.up", (numi + new->r_numinst)*sizeof(int), PM_FATAL_ERR);
    }

    for (j = 0; j < new->r_numinst; j++) {
	match = 0;
	for (i = 0; i < numi; i++) {
	    if (ilist[i] == new->r_instlist[j]) {
		match = 1;
		break;
	    }
	    if (ilist[i] > new->r_instlist[j])
		break;
	}
	if (match)
	    continue;
	for (k = numi; k > i; k--)
	    ilist[k] = ilist[k-1];
	ilist[i] = new->r_instlist[j];
	numi++;
    }

    ilist = (int *)realloc(ilist, numi*sizeof(int));
    if (ilist == NULL) {
	PM_UNLOCK(optfetch_lock);
	pmNoMem("addinst.down", numi*sizeof(int), PM_FATAL_ERR);
    }
    
    *numinst = numi;
    *instlist = ilist;

    return 1;
}

/*
 * if we retrieve the instances identified by numa and lista[], how much larger
 * is this than the set of instances identified by numb and listb[]?
 */
static int
missinst(int numa, int *lista, int numb, int *listb)
{
    int		xtra = 0;
    int		i;
    int		j;

    PM_ASSERT_IS_LOCKED(optfetch_lock);

    /* count in lista[] but _not_ in listb[] */
    if (numa == 0) {
	/* special case for all instances in lista[] */
	if (numb != 0  && numb < optcost.c_indomsize)
	    xtra += optcost.c_indomsize - numb;
    }
    else {
	/* not all instances for both lista[] and listb[] */
	i = 0;
	j = 0;
	while (i < numa && j < numb) {
	    if (lista[i] == listb[j]) {
		i++;
		j++;
	    }
	    else if (lista[i] < listb[j]) {
		i++;
		xtra++;
	    }
	    else {
		j++;
		xtra++;
	    }
	}
	xtra += (numa - i) + (numb - j);
    }

    return xtra;
}

static void
redoinst(fetchctl_t *fp)
{
    indomctl_t		*idp;
    pmidctl_t		*pmp;
    optreq_t		*rqp;

    for (idp = fp->f_idp; idp != NULL; idp = idp->i_next) {
	idp->i_numinst = -1;
	for (pmp = idp->i_pmp; pmp != NULL; pmp = pmp->p_next) {
	    pmp->p_numinst = -1;
	    for (rqp = pmp->p_rqp; rqp != NULL; rqp = rqp->r_next) {
		addinst(&pmp->p_numinst, &pmp->p_instlist, rqp);
		addinst(&idp->i_numinst, &idp->i_instlist, rqp);
	    }
	}
    }
}

static void
redopmid(fetchctl_t *fp)
{
    indomctl_t		*idp;
    pmidctl_t		*pmp;

    fp->f_numpmid = 0;
    for (idp = fp->f_idp; idp != NULL; idp = idp->i_next) {
	for (pmp = idp->i_pmp; pmp != NULL; pmp = pmp->p_next) {
	    addpmid(fp, pmp->p_pmid);
	}
    }
}

static int
optCost(fetchctl_t *fp)
{
    indomctl_t		*idp;
    indomctl_t		*xidp;
    pmidctl_t		*pmp;
    pmidctl_t		*xpmp;
    __pmID_int		*pmidp;
    __pmInDom_int	*indomp;
    int			pmd;
    int			cost = 0;
    int			done;

    PM_ASSERT_IS_LOCKED(optfetch_lock);

    /*
     * cost per PMD for the pmids in this fetch
     */
    for (idp = fp->f_idp; idp != NULL; idp = idp->i_next) {
	for (pmp = idp->i_pmp; pmp != NULL; pmp = pmp->p_next) {
	    pmidp = (__pmID_int *)&pmp->p_pmid;
	    pmd = pmidp->domain;
	    done = 0;
	    for (xidp = fp->f_idp; xidp != NULL; xidp = xidp->i_next) {
		for (xpmp = xidp->i_pmp; xpmp != NULL; xpmp = xpmp->p_next) {
		    pmidp = (__pmID_int *)&xpmp->p_pmid;
		    if (xpmp != pmp && pmd == pmidp->domain) {
			done = 1;
			break;
		    }
		    if (xpmp == pmp) {
			cost += optcost.c_pmid;
			done = 1;
			break;
		    }
		}
		if (done || xidp == idp)
		    break;
	    }
	}
    }

    /*
     * cost per PMD for the indoms in this fetch
     */
    for (idp = fp->f_idp; idp != NULL; idp = idp->i_next) {
	indomp = (__pmInDom_int *)&idp->i_indom;
	pmd = indomp->domain;
	for (xidp = fp->f_idp; xidp != idp; xidp = xidp->i_next) {
	    indomp = (__pmInDom_int *)&xidp->i_indom;
	    if (pmd == indomp->domain)
		break;
	}
	if (xidp == idp)
	    cost += optcost.c_indom;
    }

    /*
     * cost for extra retrievals due to multiple metrics over the same indom
     */
    for (idp = fp->f_idp; idp != NULL; idp = idp->i_next) {
	for (pmp = idp->i_pmp; pmp != NULL; pmp = pmp->p_next) {
	    cost += optcost.c_xtrainst * missinst(idp->i_numinst, idp->i_instlist, pmp->p_numinst, pmp->p_instlist);
	}
    }

    return cost;
}

static char *
statestr(int state, char *sbuf)
{
    sbuf[0] = '\0';
    if (state & OPT_STATE_NEW) strcat(sbuf, "NEW ");
    if (state & OPT_STATE_PMID) strcat(sbuf, "PMID ");
    if (state & OPT_STATE_PROFILE) strcat(sbuf, "PROFILE ");
    if (state & OPT_STATE_XREQ) strcat(sbuf, "XREQ ");
    if (state & OPT_STATE_XPMID) strcat(sbuf, "XPMID ");
    if (state & OPT_STATE_XINDOM) strcat(sbuf, "XINDOM ");
    if (state & OPT_STATE_XFETCH) strcat(sbuf, "XFETCH ");
    if (state & OPT_STATE_XPROFILE) strcat(sbuf, "XPROFILE ");

    return sbuf;
}

static void
dumplist(FILE *f, int style, char *tag, int numi, int *ilist)
{
    char	strbuf[20];

    fprintf(f, "%s: [%d]", tag, numi);
    if (ilist == NULL)
	fprintf(f, " (nil)\n");
    else {
	int		i;
	for (i = 0; i < numi; i++) {
	    if (style == 1)
		fprintf(f, " %s", pmIDStr_r((pmID)ilist[i], strbuf, sizeof(strbuf)));
	    else
		fprintf(f, " %d", ilist[i]);
	}
	fputc('\n', f);
    }
}

static void
___pmOptFetchDump(FILE *f, const fetchctl_t *fp)
{
    indomctl_t		*idp;
    pmidctl_t		*pmp;
    optreq_t		*rqp;
    char		strbuf[100];

    fflush(stderr);
    fflush(stdout);
    fprintf(f, "Dump optfetch structures from " PRINTF_P_PFX "%p next=" PRINTF_P_PFX "%p\n", fp, fp->f_next);
    fprintf(f, "Fetch Control @ " PRINTF_P_PFX "%p: cost=%d state=%s\n", fp, fp->f_cost, statestr(fp->f_state, strbuf));
    dumplist(f, 1, "PMIDs", fp->f_numpmid, (int *)fp->f_pmidlist);
    for (idp = fp->f_idp; idp != NULL; idp = idp->i_next) {
	fprintf(f, "  InDom %s Control @ " PRINTF_P_PFX "%p:\n", pmInDomStr_r(idp->i_indom, strbuf, sizeof(strbuf)), idp);
	dumplist(f, 0, "  instances", idp->i_numinst, idp->i_instlist);
	for (pmp = idp->i_pmp; pmp != NULL; pmp = pmp->p_next) {
	    fprintf(f, "    PMID %s Control @ " PRINTF_P_PFX "%p:\n", pmIDStr_r(pmp->p_pmid, strbuf, sizeof(strbuf)), pmp);
	    dumplist(f, 0, "    instances", pmp->p_numinst, pmp->p_instlist);
	    for (rqp = pmp->p_rqp; rqp != NULL; rqp = rqp->r_next) {
		fprintf(f, "      Request @ " PRINTF_P_PFX "%p:\n", rqp);
		dumplist(f, 0, "      instances", rqp->r_numinst, rqp->r_instlist);
	    }
	}
    }
    fputc('\n', f);
    fflush(f);
}

void
__pmOptFetchDump(FILE *f, const fetchctl_t *root)
{
    const fetchctl_t		*fp;

    for (fp = root; fp != NULL; fp = fp->f_next)
	___pmOptFetchDump(f, fp);
}

/*
 * add a new request into a group of fetches ...
 * only failure is from calloc() and this is fatal
 */
void
__pmOptFetchAdd(fetchctl_t **root, optreq_t *new)
{
    fetchctl_t		*fp;
    fetchctl_t		*tfp;
    indomctl_t		*idp;
    pmidctl_t		*pmp = NULL;
    int			mincost;
    int			change;
    pmInDom		indom = new->r_desc->indom;
    pmID		pmid = new->r_desc->pmid;

    PM_LOCK(optfetch_lock);
    /* add new fetch as first option ... will be reclaimed later if not used */
    if ((fp = (fetchctl_t *)calloc(1, sizeof(fetchctl_t))) == NULL) {
	PM_UNLOCK(optfetch_lock);
	pmNoMem("optAddFetch.fetch", sizeof(fetchctl_t), PM_FATAL_ERR);
    }
    fp->f_next = *root;
    *root = fp;

    for (fp = *root; fp != NULL; fp = fp->f_next) {
	fp->f_cost = optCost(fp);

	change = OPT_STATE_XINDOM | OPT_STATE_XPMID | OPT_STATE_XREQ;
	for (idp = fp->f_idp; idp != NULL; idp = idp->i_next) {
	    if (idp->i_indom != indom)
		continue;
	    change = OPT_STATE_XPMID | OPT_STATE_XREQ;
	    for (pmp = idp->i_pmp; pmp != NULL; pmp = pmp->p_next) {
		if (pmp->p_pmid == pmid) {
		    change = OPT_STATE_XREQ;
		    break;
		}
	    }
	    break;
	}
	if (fp == *root)
	    change |= OPT_STATE_XFETCH;
	fp->f_state = (fp->f_state & OPT_STATE_UMASK) | change;

	if (change & OPT_STATE_XINDOM) {
	    if ((idp = (indomctl_t *)calloc(1, sizeof(indomctl_t))) == NULL) {
		PM_UNLOCK(optfetch_lock);
		pmNoMem("optAddFetch.indomctl", sizeof(indomctl_t), PM_FATAL_ERR);
	    }
	    idp->i_indom = indom;
	    idp->i_next = fp->f_idp;
	    idp->i_numinst = -1;
	    fp->f_idp = idp;
	}
	if (change & OPT_STATE_XPMID) {
	    if ((pmp = (pmidctl_t *)calloc(1, sizeof(pmidctl_t))) == NULL) {
		PM_UNLOCK(optfetch_lock);
		pmNoMem("optAddFetch.pmidctl", sizeof(pmidctl_t), PM_FATAL_ERR);
	    }
	    pmp->p_next = idp->i_pmp;
	    idp->i_pmp = pmp;
	    pmp->p_pmid = pmid;
	    pmp->p_numinst = -1;
	}
	addinst(&pmp->p_numinst, &pmp->p_instlist, new);
	if (addinst(&idp->i_numinst, &idp->i_instlist, new))
	    fp->f_state |= OPT_STATE_XPROFILE;

	fp->f_newcost = optCost(fp);
	if (fp == *root)
	    fp->f_newcost += optcost.c_fetch;
	if (pmDebugOptions.optfetch && pmDebugOptions.desperate) {
	    char	strbuf[100];
	    fprintf(stderr, "__pmOptFetchAdd: fp=" PRINTF_P_PFX "%p cost=", fp);
	    if (fp->f_cost == OPT_COST_INFINITY)
		fprintf(stderr, "INFINITY");
	    else
		fprintf(stderr, "%d", fp->f_cost);
	    fprintf(stderr, ", newcost=");
	    if (fp->f_newcost == OPT_COST_INFINITY)
		fprintf(stderr, "INFINITY");
	    else
		fprintf(stderr, "%d", fp->f_newcost);
	    fprintf(stderr, ", for %s @ grp " PRINTF_P_PFX "%p,",
		pmIDStr_r(pmid, strbuf, sizeof(strbuf)), fp);
	    fprintf(stderr, " state %s\n",
		statestr(fp->f_state, strbuf));
	}
    }

    tfp = NULL;
    mincost = OPT_COST_INFINITY;
    for (fp = *root; fp != NULL; fp = fp->f_next) {
	int		cost;
	if (optcost.c_scope)
	    /* global */
	    cost = fp->f_newcost;
	else
	    /* local */
	    cost = fp->f_newcost - fp->f_cost;
	if (cost < mincost) {
	    mincost = cost;
	    tfp = fp;
	}
    }
    if (pmDebugOptions.optfetch && pmDebugOptions.desperate) {
	char	strbuf[100];
	fprintf(stderr, "__pmOptFetchAdd: fp=" PRINTF_P_PFX "%p chose %s cost=%d for %s @ grp " PRINTF_P_PFX "%p,",
		tfp, optcost.c_scope ? "global" : "incremental",
		mincost, pmIDStr_r(pmid, strbuf, sizeof(strbuf)), tfp);
	fprintf(stderr, " change %s\n", statestr(tfp->f_state, strbuf));
    }

    /*
     * Warning! Traversal of the list is a bit tricky, because the
     *		current element (fp) may be freed, making fp->fp_next
     *		a bad idea at the end of each iteration ...
     */
    for (fp = *root; fp != NULL; ) {
	for (idp = fp->f_idp; idp != NULL; idp = idp->i_next) {
	    if (idp->i_indom != indom)
		continue;
	    for (pmp = idp->i_pmp; pmp != NULL; pmp = pmp->p_next) {
		if (pmp->p_pmid == pmid)
		    break;
	    }
	    break;
	}
	assert(idp != NULL && pmp != NULL);
	if (fp == tfp) {
	    /*
	     * The chosen one ...
	     */
	    if (fp->f_state & OPT_STATE_XFETCH)
		fp->f_state |= OPT_STATE_NEW;
	    if (addpmid(tfp, pmid))
		fp->f_state |= OPT_STATE_PMID;
	    if (fp->f_state & OPT_STATE_XPROFILE)
		fp->f_state |= OPT_STATE_PROFILE;
	    new->r_next = pmp->p_rqp;
	    new->r_fetch = tfp;
	    pmp->p_rqp = new;
	    fp->f_cost = fp->f_newcost;
	    fp->f_state &= OPT_STATE_UMASK;
	    fp = fp->f_next;
	}
	else {
	    /*
	     * Otherwise, need to undo changes made to data structures.
	     * Note.  if new structures added, they will be at the start of
	     *        their respective lists.
	     */
	    if (fp->f_state & OPT_STATE_XPMID) {
		idp->i_pmp = pmp->p_next;
		free(pmp);
	    }
	    if (fp->f_state & OPT_STATE_XINDOM) {
		fp->f_idp = idp->i_next;
		free(idp);
	    }
	    if (fp->f_state & OPT_STATE_XFETCH) {
		*root = fp->f_next;
		free(fp);
		fp = *root;
	    }
	    else {
		redoinst(fp);
		fp->f_state &= OPT_STATE_UMASK;
		fp = fp->f_next;
	    }
	}
    }

    PM_UNLOCK(optfetch_lock);
    return;
}

/*
 * remove a request from a group of fetches
 */
int
__pmOptFetchDel(fetchctl_t **root, optreq_t *new)
{
    fetchctl_t		*fp;
    fetchctl_t		*p_fp;
    indomctl_t		*idp;
    indomctl_t		*p_idp;
    pmidctl_t		*pmp;
    pmidctl_t		*p_pmp;
    optreq_t		*rqp;
    optreq_t		*p_rqp;

    if (pmDebugOptions.optfetch && pmDebugOptions.desperate) {
	fprintf(stderr, "__pmOptFetchDel: " PRINTF_P_PFX "%p\n", new);
    }

    p_fp = NULL;
    for (fp = *root; fp != NULL; fp = fp->f_next) {
	p_idp = NULL;
	for (idp = fp->f_idp; idp != NULL; idp = idp->i_next) {
	    p_pmp = NULL;
	    for (pmp = idp->i_pmp; pmp != NULL; pmp = pmp->p_next) {
		p_rqp = NULL;
		for (rqp = pmp->p_rqp; rqp != NULL; rqp = rqp->r_next) {
		    if (rqp == new) {
			if (p_rqp != NULL)
			    /* not first request for this metric */
			    p_rqp->r_next = rqp->r_next;
			else if (rqp->r_next != NULL)
			    /* first of several requests for this metric */
			    pmp->p_rqp = rqp->r_next;
			else {
			    /* only request for this metric */
			    if (p_pmp != NULL)
				/* not first metric for this indom */
				p_pmp->p_next = pmp->p_next;
			    else if (pmp->p_next != NULL)
				/* first of several metrics for this indom */
				idp->i_pmp = pmp->p_next;
			    else {
				/* only metric for this indom */
				if (p_idp != NULL)
				    /* not first indom for this fetch */
				    p_idp->i_next = idp->i_next;
				else if (idp->i_next != NULL)
				    /* first of several idoms for this fetch */
				    fp->f_idp = idp->i_next;
				else {
				    /* only indom for this fetch */
				    if (p_fp != NULL)
					/* not first fetch for the group */
					p_fp->f_next = fp->f_next;
				    else 
					/* first of fetch for the group */
					*root = fp->f_next;
				    free(fp);
				    fp = NULL;
				}
				free(idp);
			    }
			    free(pmp);
			}
			/* data structures repaired, now redo lists */
			if (fp != NULL) {
			    redoinst(fp);
			    redopmid(fp);
			    fp->f_state = OPT_STATE_PMID | OPT_STATE_PROFILE;
			    if (pmDebugOptions.optfetch && pmDebugOptions.desperate) {


				fprintf(stderr, "__pmOptFetchDel: redo " PRINTF_P_PFX "%p old cost=", fp);
				if (fp->f_cost == OPT_COST_INFINITY)
				    fprintf(stderr, "INFINITY");
				else
				    fprintf(stderr, "%d", fp->f_cost);
			    }
			    PM_LOCK(optfetch_lock);
			    fp->f_cost = optCost(fp);
			    PM_UNLOCK(optfetch_lock);
			    if (pmDebugOptions.optfetch && pmDebugOptions.desperate) {
				fprintf(stderr, " new cost=");
				if (fp->f_cost == OPT_COST_INFINITY)
				    fprintf(stderr, "INFINITY");
				else
				    fprintf(stderr, "%d", fp->f_cost);
				fputc('\n', stderr);
			    }
			}
			return 0;
		    }
		    p_rqp = rqp;
		}
		p_pmp = pmp;
	    }
	    p_idp = idp;
	}
	p_fp = fp;
    }
    return -1;
}

void
__pmOptFetchRedo(fetchctl_t **root)
{
    fetchctl_t		*newroot = NULL;
    fetchctl_t		*fp;
    fetchctl_t		*t_fp;
    indomctl_t		*idp;
    indomctl_t		*t_idp;
    pmidctl_t		*pmp;
    pmidctl_t		*t_pmp;
    optreq_t		*rqp;
    optreq_t		*t_rqp;
    optreq_t		*p_rqp;
    optreq_t		*rlist;
    int			numreq;

    rlist = NULL;
    numreq = 0;
    /*
     * collect all of the requests first
     */
    for (fp = *root; fp != NULL; ) {
	for (idp = fp->f_idp; idp != NULL; ) {
	    for (pmp = idp->i_pmp; pmp != NULL; ) {
		for (rqp = pmp->p_rqp; rqp != NULL; ) {
		    t_rqp = rqp->r_next;
		    rqp->r_next = rlist;
		    rlist = rqp;
		    rqp = t_rqp;
		    numreq++;
		}
		t_pmp = pmp;
		pmp = pmp->p_next;
		free(t_pmp);
	    }
	    t_idp = idp;
	    idp = idp->i_next;
	    free(t_idp);
	}
	t_fp = fp;
	fp = fp->f_next;
	free(t_fp);
    }

    if (numreq) {
	/* something to do, randomly cut and splice the list of requests */
	numreq = (int)lrand48() % numreq;	/* THREADSAFE - don't care */
	t_rqp = rlist;
	p_rqp = NULL;
	for (rqp = rlist; rqp != NULL; rqp = rqp->r_next) {
	    if (numreq == 0)
		t_rqp = rqp;
	    numreq--;
	    p_rqp = rqp;
	}
	if (p_rqp == NULL || t_rqp == rlist)
	    /* do nothing */
	    ;
	else {
	    /* p_rqp is end of list, t_rqp is new head */
	    p_rqp->r_next = rlist;
	    rlist = t_rqp->r_next;
	    t_rqp->r_next = NULL;
	}

	/* now add them all back again */
	for (rqp = rlist; rqp != NULL; ) {
	    /* warning, rqp->r_next may change */
	    t_rqp = rqp->r_next;
	    __pmOptFetchAdd(&newroot, rqp);
	    rqp = t_rqp;
	}
    }

    *root = newroot;
    return;
}

void
__pmOptFetchGetParams(optcost_t *ocp)
{
    PM_LOCK(optfetch_lock);
    *ocp = optcost;
    PM_UNLOCK(optfetch_lock);
    return;
}

void
__pmOptFetchPutParams(optcost_t *ocp)
{
    PM_LOCK(optfetch_lock);
    optcost = *ocp;
    PM_UNLOCK(optfetch_lock);
    return;
}
