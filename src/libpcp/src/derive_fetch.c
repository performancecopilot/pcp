/*
 * Copyright (c) 2009,2014 Ken McDonell.  All Rights Reserved.
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
 * Debug Flags
 *	DERIVE - high-level diagnostics
 *	DERIVE & APPL0 - configuration and static syntax analysis
 *	DERIVE & APPL1 - expression binding and semantic analysis
 *	DERIVE & APPL2 - fetch handling
 */

#include <inttypes.h>
#include <assert.h>
#include "pmapi.h"
#include "impl.h"
#include "internal.h"

static void
get_pmids(node_t *np, int *cnt, pmID **list)
{
    assert(np != NULL);
    if (np->left != NULL) get_pmids(np->left, cnt, list);
    if (np->right != NULL) get_pmids(np->right, cnt, list);
    if (np->type == L_NAME) {
	(*cnt)++;
	if ((*list = (pmID *)realloc(*list, (*cnt)*sizeof(pmID))) == NULL) {
	    __pmNoMem("__dmprefetch: realloc xtralist", (*cnt)*sizeof(pmID), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	(*list)[*cnt-1] = np->info->pmid;
    }
}

/*
 * Walk the pmidlist[] from pmFetch.
 * For each derived metric found in the list add all the operand metrics,
 * and build a combined pmID list (newlist).
 *
 * Return 0 if no derived metrics in the list, else the number of pmIDs
 * in the combined list.
 *
 * The derived metric pmIDs are left in the combined list (they will
 * return PM_ERR_NOAGENT from the fetch) to simplify the post-processing
 * of the pmResult in __dmpostfetch()
 */
int
__dmprefetch(__pmContext *ctxp, int numpmid, const pmID *pmidlist, pmID **newlist)
{
    int		i;
    int		j;
    int		m;
    int		xtracnt = 0;
    pmID	*xtralist = NULL;
    pmID	*list;
    ctl_t	*cp = (ctl_t *)ctxp->c_dm;

    /* if needed, init() called in __dmopencontext beforehand */

    if (cp == NULL) return 0;

    /*
     * save numpmid to be used in __dmpostfetch() ... works because calls
     * to pmFetch cannot be nested (at all, but certainly for the same
     * context).
     * Ditto for the fast path flag (fetch_has_dm).
     */
    cp->numpmid = numpmid;
    cp->fetch_has_dm = 0;

    for (m = 0; m < numpmid; m++) {
	if (pmid_domain(pmidlist[m]) != DYNAMIC_PMID ||
	    pmid_item(pmidlist[m]) == 0)
	    continue;
	for (i = 0; i < cp->nmetric; i++) {
	    if (pmidlist[m] == cp->mlist[i].pmid) {
		if (cp->mlist[i].expr != NULL) {
		    get_pmids(cp->mlist[i].expr, &xtracnt, &xtralist);
		    cp->fetch_has_dm = 1;
		}
		break;
	    }
	}
    }
    if (xtracnt == 0) {
	if (cp->fetch_has_dm)
	    return numpmid;
	else
	    return 0;
    }

    /*
     * Some of the "extra" ones, may already be in the caller's pmFetch 
     * list, or repeated in xtralist[] (if the same metric operand appears
     * more than once as a leaf node in the expression tree.
     * Remove these duplicates
     */
    j = 0;
    for (i = 0; i < xtracnt; i++) {
	for (m = 0; m < numpmid; m++) {
	    if (xtralist[i] == pmidlist[m])
		/* already in pmFetch list */
		break;
	}
	if (m < numpmid) continue;
	for (m = 0; m < j; m++) {
	    if (xtralist[i] == xtralist[m])
	    	/* already in xtralist[] */
		break;
	}
	if (m == j)
	    xtralist[j++] = xtralist[i];
    }
    xtracnt = j;
    if (xtracnt == 0) {
	free(xtralist);
	return numpmid;
    }

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL2)) {
	char	strbuf[20];
	fprintf(stderr, "derived metrics prefetch added %d metrics:", xtracnt);
	for (i = 0; i < xtracnt; i++)
	    fprintf(stderr, " %s", pmIDStr_r(xtralist[i], strbuf, sizeof(strbuf)));
	fputc('\n', stderr);
    }
#endif
    if ((list = (pmID *)malloc((numpmid+xtracnt)*sizeof(pmID))) == NULL) {
	__pmNoMem("__dmprefetch: alloc list", (numpmid+xtracnt)*sizeof(pmID), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    for (m = 0; m < numpmid; m++) {
	list[m] = pmidlist[m];
    }
    for (i = 0; i < xtracnt; i++) {
	list[m++] = xtralist[i];
    }
    free(xtralist);
    *newlist = list;

    return m;
}

/*
 * Free the old ivlist[] (if any) ... may need to walk the list because
 * the pmAtomValues may have buffers attached in the type STRING,
 * type AGGREGATE* and type EVENT cases.
 * Includes logic to save one history sample (for delta() and rate()).
 */
static void
free_ivlist(node_t *np)
{
    int		i;

    assert(np->info != NULL);

    if (np->save_last) {
	/*
	 * saving history for delta() or rate() ... release previous
	 * sample, and save this sample
	 */
	if (np->info->last_ivlist != NULL) {
	    /*
	     * no STRING, AGGREGATE or EVENT types for delta() or rate()
	     * so simple free()
	     */
	    free(np->info->last_ivlist);
	}
	np->info->last_numval = np->info->numval;
	np->info->last_ivlist = np->info->ivlist;
	np->info->ivlist = NULL;
    }
    else {
	/* no history */
	if (np->info->ivlist != NULL) {
	    if (np->desc.type == PM_TYPE_STRING) {
		for (i = 0; i < np->info->numval; i++) {
		    if (np->info->ivlist[i].value.cp != NULL)
			free(np->info->ivlist[i].value.cp);
		}
	    }
	    else if (np->desc.type == PM_TYPE_AGGREGATE ||
		     np->desc.type == PM_TYPE_AGGREGATE_STATIC ||
		     np->desc.type == PM_TYPE_EVENT ||
		     np->desc.type == PM_TYPE_HIGHRES_EVENT) {
		for (i = 0; i < np->info->numval; i++) {
		    if (np->info->ivlist[i].value.vbp != NULL)
			free(np->info->ivlist[i].value.vbp);
		}
	    }
	}
	free(np->info->ivlist);
	np->info->numval = 0;
	np->info->ivlist = NULL;
    }
}

/*
 * Binary arithmetic.
 *
 * result = <a> <op> <b>
 * ltype, rtype and type are the types of <a>, <b> and the result
 * respectively
 *
 * If type is PM_TYPE_DOUBLE then lmul, ldiv, rmul and rdiv are
 * the scale factors for units scale conversion of <a> and <b>
 * respectively, so lmul*<a>/ldiv ... all are 1 in the common cases.
 */
static pmAtomValue
bin_op(int type, int op, pmAtomValue a, int ltype, int lmul, int ldiv, pmAtomValue b, int rtype, int rmul, int rdiv)
{
    pmAtomValue	res;
    pmAtomValue	l;
    pmAtomValue	r;

    l = a;	/* struct assignments */
    r = b;

    /* 
     * Promote each operand to the type of the result ... there are limited
     * cases to be considered here, see promote[][] and map_desc().
     */
    switch (type) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	    /* do nothing */
	    break;
	case PM_TYPE_64:
	    switch (ltype) {
		case PM_TYPE_32:
		    l.ll = a.l;
		    break;
		case PM_TYPE_U32:
		    l.ll = a.ul;
		    break;
		case PM_TYPE_64:
		case PM_TYPE_U64:
		    /* do nothing */
		    break;
	    }
	    switch (rtype) {
		case PM_TYPE_32:
		    r.ll = b.l;
		    break;
		case PM_TYPE_U32:
		    r.ll = b.ul;
		    break;
		case PM_TYPE_64:
		case PM_TYPE_U64:
		    /* do nothing */
		    break;
	    }
	    break;
	case PM_TYPE_U64:
	    switch (ltype) {
		case PM_TYPE_32:
		    l.ull = a.l;
		    break;
		case PM_TYPE_U32:
		    l.ull = a.ul;
		    break;
		case PM_TYPE_64:
		case PM_TYPE_U64:
		    /* do nothing */
		    break;
	    }
	    switch (rtype) {
		case PM_TYPE_32:
		    r.ull = b.l;
		    break;
		case PM_TYPE_U32:
		    r.ull = b.ul;
		    break;
		case PM_TYPE_64:
		case PM_TYPE_U64:
		    /* do nothing */
		    break;
	    }
	    break;
	case PM_TYPE_FLOAT:
	    switch (ltype) {
		case PM_TYPE_32:
		    l.f = a.l;
		    break;
		case PM_TYPE_U32:
		    l.f = a.ul;
		    break;
		case PM_TYPE_64:
		    l.f = a.ll;
		    break;
		case PM_TYPE_U64:
		    l.f = a.ull;
		    break;
		case PM_TYPE_FLOAT:
		    /* do nothing */
		    break;
	    }
	    switch (rtype) {
		case PM_TYPE_32:
		    r.f = b.l;
		    break;
		case PM_TYPE_U32:
		    r.f = b.ul;
		    break;
		case PM_TYPE_64:
		    r.f = b.ll;
		    break;
		case PM_TYPE_U64:
		    r.f = b.ull;
		    break;
		case PM_TYPE_FLOAT:
		    /* do nothing */
		    break;
	    }
	    break;
	case PM_TYPE_DOUBLE:
	    switch (ltype) {
		case PM_TYPE_32:
		    l.d = a.l;
		    break;
		case PM_TYPE_U32:
		    l.d = a.ul;
		    break;
		case PM_TYPE_64:
		    l.d = a.ll;
		    break;
		case PM_TYPE_U64:
		    l.d = a.ull;
		    break;
		case PM_TYPE_FLOAT:
		    l.d = a.f;
		    break;
		case PM_TYPE_DOUBLE:
		    /* do nothing */
		    break;
	    }
	    l.d = (l.d / ldiv) * lmul;
	    switch (rtype) {
		case PM_TYPE_32:
		    r.d = b.l;
		    break;
		case PM_TYPE_U32:
		    r.d = b.ul;
		    break;
		case PM_TYPE_64:
		    r.d = b.ll;
		    break;
		case PM_TYPE_U64:
		    r.d = b.ull;
		    break;
		case PM_TYPE_FLOAT:
		    r.d = b.f;
		    break;
		case PM_TYPE_DOUBLE:
		    /* do nothing */
		    break;
	    }
	    r.d = (r.d / rdiv) * rmul;
	    break;
    }

    /*
     * Do the aritmetic ... messy!
     */
    switch (type) {
	case PM_TYPE_32:
	    switch (op) {
		case L_PLUS:
		    res.l = l.l + r.l;
		    break;
		case L_MINUS:
		    res.l = l.l - r.l;
		    break;
		case L_STAR:
		    res.l = l.l * r.l;
		    break;
		/* semantics enforce no L_SLASH for integer results */
	    }
	    break;
	case PM_TYPE_U32:
	    switch (op) {
		case L_PLUS:
		    res.ul = l.ul + r.ul;
		    break;
		case L_MINUS:
		    res.ul = l.ul - r.ul;
		    break;
		case L_STAR:
		    res.ul = l.ul * r.ul;
		    break;
		/* semantics enforce no L_SLASH for integer results */
	    }
	    break;
	case PM_TYPE_64:
	    switch (op) {
		case L_PLUS:
		    res.ll = l.ll + r.ll;
		    break;
		case L_MINUS:
		    res.ll = l.ll - r.ll;
		    break;
		case L_STAR:
		    res.ll = l.ll * r.ll;
		    break;
		/* semantics enforce no L_SLASH for integer results */
	    }
	    break;
	case PM_TYPE_U64:
	    switch (op) {
		case L_PLUS:
		    res.ull = l.ull + r.ull;
		    break;
		case L_MINUS:
		    res.ull = l.ull - r.ull;
		    break;
		case L_STAR:
		    res.ull = l.ull * r.ull;
		    break;
		/* semantics enforce no L_SLASH for integer results */
	    }
	    break;
	case PM_TYPE_FLOAT:
	    switch (op) {
		case L_PLUS:
		    res.f = l.f + r.f;
		    break;
		case L_MINUS:
		    res.f = l.f - r.f;
		    break;
		case L_STAR:
		    res.f = l.f * r.f;
		    break;
		/* semantics enforce no L_SLASH for float results */
	    }
	    break;
	case PM_TYPE_DOUBLE:
	    switch (op) {
		case L_PLUS:
		    res.d = l.d + r.d;
		    break;
		case L_MINUS:
		    res.d = l.d - r.d;
		    break;
		case L_STAR:
		    res.d = l.d * r.d;
		    break;
		case L_SLASH:
		    if (l.d == 0)
			res.d = 0;
		    else
			res.d = l.d / r.d;
		    break;
	    }
	    break;
    }

    return res;
}


/*
 * Walk an expression tree, filling in operand values from the
 * pmResult at the leaf nodes and propagating the computed values
 * towards the root node of the tree.
 */
static int
eval_expr(node_t *np, pmResult *rp, int level)
{
    int		sts;
    int		i;
    int		j;
    int		k;
    size_t	need;

    assert(np != NULL);
    if (np->left != NULL) {
	sts = eval_expr(np->left, rp, level+1);
	if (sts < 0) return sts;
    }
    if (np->right != NULL) {
	sts = eval_expr(np->right, rp, level+1);
	if (sts < 0) return sts;
    }

    /* mostly, np->left is not NULL ... */
    assert (np->type == L_NUMBER || np->type == L_NAME || np->left != NULL);

    switch (np->type) {

	case L_NUMBER:
	    if (np->info->numval == 0) {
		/* initialize ivlist[] for singular instance first time through */
		np->info->numval = 1;
		if ((np->info->ivlist = (val_t *)malloc(sizeof(val_t))) == NULL) {
		    __pmNoMem("eval_expr: number ivlist", sizeof(val_t), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		np->info->ivlist[0].inst = PM_INDOM_NULL;
		/* don't need error checking, done in the lexical scanner */
		np->info->ivlist[0].value.l = atoi(np->value);
	    }
	    return 1;
	    break;

	case L_DELTA:
	case L_RATE:
	    /*
	     * this and the last values are in the left expr
	     */
	    np->info->last_stamp = np->info->stamp;
	    np->info->stamp = rp->timestamp;
	    free_ivlist(np);
	    np->info->numval = np->left->info->numval <= np->left->info->last_numval ? np->left->info->numval : np->left->info->last_numval;
	    if (np->info->numval <= 0)
		return np->info->numval;
	    if ((np->info->ivlist = (val_t *)malloc(np->info->numval*sizeof(val_t))) == NULL) {
		__pmNoMem("eval_expr: delta()/rate() ivlist", np->info->numval*sizeof(val_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    /*
	     * delta()
	     * ivlist[k] = left->ivlist[i] - left->last_ivlist[j]
	     * rate()
	     * ivlist[k] = (left->ivlist[i] - left->last_ivlist[j]) /
	     *             (timestamp - left->last_stamp)
	     */
	    for (i = k = 0; i < np->left->info->numval; i++) {
		j = i;
		if (j >= np->left->info->last_numval)
		    j = 0;
		if (np->left->info->ivlist[i].inst != np->left->info->last_ivlist[j].inst) {
		    /* current ith inst != last jth inst ... search in last */
#ifdef PCP_DEBUG
		    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL2)) {
			fprintf(stderr, "eval_expr: inst[%d] mismatch left [%d]=%d last [%d]=%d\n", k, i, np->left->info->ivlist[i].inst, j, np->left->info->last_ivlist[j].inst);
		    }
#endif
		    for (j = 0; j < np->left->info->last_numval; j++) {
			if (np->left->info->ivlist[i].inst == np->left->info->last_ivlist[j].inst)
			    break;
		    }
		    if (j == np->left->info->last_numval) {
			/* no match, skip this instance from this result */
			continue;
		    }
#ifdef PCP_DEBUG
		    else {
			if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL2)) {
			    fprintf(stderr, "eval_expr: recover @ last [%d]=%d\n", j, np->left->info->last_ivlist[j].inst);
			}
		    }
#endif
		}
		np->info->ivlist[k].inst = np->left->info->ivlist[i].inst;
		if (np->type == L_DELTA) {
		    /* for delta() result type == operand type */
		    switch (np->left->desc.type) {
			case PM_TYPE_32:
			    np->info->ivlist[k].value.l = np->left->info->ivlist[i].value.l - np->left->info->last_ivlist[j].value.l;
			    break;
			case PM_TYPE_U32:
			    np->info->ivlist[k].value.ul = np->left->info->ivlist[i].value.ul - np->left->info->last_ivlist[j].value.ul;
			    break;
			case PM_TYPE_64:
			    np->info->ivlist[k].value.ll = np->left->info->ivlist[i].value.ll - np->left->info->last_ivlist[j].value.ll;
			    break;
			case PM_TYPE_U64:
			    np->info->ivlist[k].value.ull = np->left->info->ivlist[i].value.ull - np->left->info->last_ivlist[j].value.ull;
			    break;
			case PM_TYPE_FLOAT:
			    np->info->ivlist[k].value.f = np->left->info->ivlist[i].value.f - np->left->info->last_ivlist[j].value.f;
			    break;
			case PM_TYPE_DOUBLE:
			    np->info->ivlist[k].value.d = np->left->info->ivlist[i].value.d - np->left->info->last_ivlist[j].value.d;
			    break;
			default:
			    /*
			     * Nothing should end up here as check_expr() checks
			     * for numeric data type at bind time
			     */
			    return PM_ERR_CONV;
		    }
		}
		else {
		    /* rate() conversion, type will be DOUBLE */
		    struct timeval	stampdiff;
		    stampdiff = np->info->stamp;
		    __pmtimevalDec(&stampdiff, &np->info->last_stamp);
		    switch (np->left->desc.type) {
			case PM_TYPE_32:
			    np->info->ivlist[k].value.d = (double)(np->left->info->ivlist[i].value.l - np->left->info->last_ivlist[j].value.l);
			    break;
			case PM_TYPE_U32:
			    np->info->ivlist[k].value.d = (double)(np->left->info->ivlist[i].value.ul - np->left->info->last_ivlist[j].value.ul);
			    break;
			case PM_TYPE_64:
			    np->info->ivlist[k].value.d = (double)(np->left->info->ivlist[i].value.ll - np->left->info->last_ivlist[j].value.ll);
			    break;
			case PM_TYPE_U64:
			    np->info->ivlist[k].value.d = (double)(np->left->info->ivlist[i].value.ull - np->left->info->last_ivlist[j].value.ull);
			    break;
			case PM_TYPE_FLOAT:
			    np->info->ivlist[k].value.d = (double)(np->left->info->ivlist[i].value.f - np->left->info->last_ivlist[j].value.f);
			    break;
			case PM_TYPE_DOUBLE:
			    np->info->ivlist[k].value.d = np->left->info->ivlist[i].value.d - np->left->info->last_ivlist[j].value.d;
			    break;
			default:
			    /*
			     * Nothing should end up here as check_expr() checks
			     * for numeric data type at bind time
			     */
			    return PM_ERR_CONV;
		    }
		    np->info->ivlist[k].value.d /= __pmtimevalToReal(&stampdiff);
		    /*
		     * check_expr() ensures dimTime is 0 or 1 at bind time
		     */
		    if (np->left->desc.units.dimTime == 1) {
			/* scale rate(time counter) -> time utilization */
			if (np->info->time_scale < 0) {
			    /*
			     * one trip initialization for time utilization
			     * scaling factor (to scale metric from counter
			     * units into seconds)
			     */
			    int		i;
			    np->info->time_scale = 1;
			    if (np->left->desc.units.scaleTime > PM_TIME_SEC) {

				for (i = PM_TIME_SEC; i < np->left->desc.units.scaleTime; i++)

				    np->info->time_scale *= 60;
			    }
			    else {
				for (i = np->left->desc.units.scaleTime; i < PM_TIME_SEC; i++)
				    np->info->time_scale /= 1000;
			    }
			}
			np->info->ivlist[k].value.d *= np->info->time_scale;
		    }
		}
		k++;
	    }
	    np->info->numval = k;
	    return np->info->numval;
	    break;

	case L_INSTANT:
	    /*
	     * values are in the left expr
	     */
	    np->info->last_stamp = np->info->stamp;
	    np->info->stamp = rp->timestamp;
	    np->info->numval = np->left->info->numval;
	    if (np->info->numval > 0)
		np->info->ivlist = np->left->info->ivlist;
	    return np->info->numval;
	    break;

	case L_AVG:
	case L_COUNT:
	case L_SUM:
	case L_MAX:
	case L_MIN:
	    if (np->info->ivlist == NULL) {
		/* initialize ivlist[] for singular instance first time through */
		if ((np->info->ivlist = (val_t *)malloc(sizeof(val_t))) == NULL) {
		    __pmNoMem("eval_expr: aggr ivlist", sizeof(val_t), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		np->info->ivlist[0].inst = PM_IN_NULL;
	    }
	    /*
	     * values are in the left expr
	     */
	    if (np->type == L_COUNT) {
		np->info->numval = 1;
		np->info->ivlist[0].value.l = np->left->info->numval;
	    }
	    else {
		np->info->numval = 1;
		if (np->type == L_AVG)
		    np->info->ivlist[0].value.f = 0;
		else if (np->type == L_SUM) {
		    switch (np->desc.type) {
			case PM_TYPE_32:
			    np->info->ivlist[0].value.l = 0;
			    break;
			case PM_TYPE_U32:
			    np->info->ivlist[0].value.ul = 0;
			    break;
			case PM_TYPE_64:
			    np->info->ivlist[0].value.ll = 0;
			    break;
			case PM_TYPE_U64:
			    np->info->ivlist[0].value.ull = 0;
			    break;
			case PM_TYPE_FLOAT:
			    np->info->ivlist[0].value.f = 0;
			    break;
			case PM_TYPE_DOUBLE:
			    np->info->ivlist[0].value.d = 0;
			    break;
		    }
		}
		for (i = 0; i < np->left->info->numval; i++) {
		    switch (np->type) {

			case L_AVG:
			    switch (np->left->desc.type) {
				case PM_TYPE_32:
				    np->info->ivlist[0].value.f += (float)np->left->info->ivlist[i].value.l / np->left->info->numval;
				    break;
				case PM_TYPE_U32:
				    np->info->ivlist[0].value.f += (float)np->left->info->ivlist[i].value.ul / np->left->info->numval;
				    break;
				case PM_TYPE_64:
				    np->info->ivlist[0].value.f += (float)np->left->info->ivlist[i].value.ll / np->left->info->numval;
				    break;
				case PM_TYPE_U64:
				    np->info->ivlist[0].value.f += (float)np->left->info->ivlist[i].value.ull / np->left->info->numval;
				    break;
				case PM_TYPE_FLOAT:
				    np->info->ivlist[0].value.f += (float)np->left->info->ivlist[i].value.f / np->left->info->numval;
				    break;
				case PM_TYPE_DOUBLE:
				    np->info->ivlist[0].value.f += (float)np->left->info->ivlist[i].value.d / np->left->info->numval;
				    break;
				default:
				    /*
				     * check_expr() checks for numeric data
				     * type at bind time ... if here, botch!
				     */
				    return PM_ERR_CONV;
			    }
			    break;

			case L_MAX:
			    switch (np->desc.type) {
				case PM_TYPE_32:
				    if (i == 0 ||
				        np->info->ivlist[0].value.l < np->left->info->ivlist[i].value.l)
					np->info->ivlist[0].value.l = np->left->info->ivlist[i].value.l;
				    break;
				case PM_TYPE_U32:
				    if (i == 0 ||
				        np->info->ivlist[0].value.ul < np->left->info->ivlist[i].value.ul)
					np->info->ivlist[0].value.ul = np->left->info->ivlist[i].value.ul;
				    break;
				case PM_TYPE_64:
				    if (i == 0 ||
				        np->info->ivlist[0].value.ll < np->left->info->ivlist[i].value.ll)
					np->info->ivlist[0].value.ll = np->left->info->ivlist[i].value.ll;
				    break;
				case PM_TYPE_U64:
				    if (i == 0 ||
				        np->info->ivlist[0].value.ull < np->left->info->ivlist[i].value.ull)
					np->info->ivlist[0].value.ull = np->left->info->ivlist[i].value.ull;
				    break;
				case PM_TYPE_FLOAT:
				    if (i == 0 ||
				        np->info->ivlist[0].value.f < np->left->info->ivlist[i].value.f)
					np->info->ivlist[0].value.f = np->left->info->ivlist[i].value.f;
				    break;
				case PM_TYPE_DOUBLE:
				    if (i == 0 ||
				        np->info->ivlist[0].value.d < np->left->info->ivlist[i].value.d)
					np->info->ivlist[0].value.d = np->left->info->ivlist[i].value.d;
				    break;
				default:
				    /*
				     * check_expr() checks for numeric data
				     * type at bind time ... if here, botch!
				     */
				    return PM_ERR_CONV;
			    }
			    break;

			case L_MIN:
			    switch (np->desc.type) {
				case PM_TYPE_32:
				    if (i == 0 ||
				        np->info->ivlist[0].value.l > np->left->info->ivlist[i].value.l)
					np->info->ivlist[0].value.l = np->left->info->ivlist[i].value.l;
				    break;
				case PM_TYPE_U32:
				    if (i == 0 ||
				        np->info->ivlist[0].value.ul > np->left->info->ivlist[i].value.ul)
					np->info->ivlist[0].value.ul = np->left->info->ivlist[i].value.ul;
				    break;
				case PM_TYPE_64:
				    if (i == 0 ||
				        np->info->ivlist[0].value.ll > np->left->info->ivlist[i].value.ll)
					np->info->ivlist[0].value.ll = np->left->info->ivlist[i].value.ll;
				    break;
				case PM_TYPE_U64:
				    if (i == 0 ||
				        np->info->ivlist[0].value.ull > np->left->info->ivlist[i].value.ull)
					np->info->ivlist[0].value.ull = np->left->info->ivlist[i].value.ull;
				    break;
				case PM_TYPE_FLOAT:
				    if (i == 0 ||
				        np->info->ivlist[0].value.f > np->left->info->ivlist[i].value.f)
					np->info->ivlist[0].value.f = np->left->info->ivlist[i].value.f;
				    break;
				case PM_TYPE_DOUBLE:
				    if (i == 0 ||
				        np->info->ivlist[0].value.d > np->left->info->ivlist[i].value.d)
					np->info->ivlist[0].value.d = np->left->info->ivlist[i].value.d;
				    break;
				default:
				    /*
				     * check_expr() checks for numeric data
				     * type at bind time ... if here, botch!
				     */
				    return PM_ERR_CONV;
			    }
			    break;

			case L_SUM:
			    switch (np->desc.type) {
				case PM_TYPE_32:
				    np->info->ivlist[0].value.l += np->left->info->ivlist[i].value.l;
				    break;
				case PM_TYPE_U32:
				    np->info->ivlist[0].value.ul += np->left->info->ivlist[i].value.ul;
				    break;
				case PM_TYPE_64:
				    np->info->ivlist[0].value.ll += np->left->info->ivlist[i].value.ll;
				    break;
				case PM_TYPE_U64:
				    np->info->ivlist[0].value.ull += np->left->info->ivlist[i].value.ull;
				    break;
				case PM_TYPE_FLOAT:
				    np->info->ivlist[0].value.f += np->left->info->ivlist[i].value.f;
				    break;
				case PM_TYPE_DOUBLE:
				    np->info->ivlist[0].value.d += np->left->info->ivlist[i].value.d;
				    break;
				default:
				    /*
				     * check_expr() checks for numeric data
				     * type at bind time ... if here, botch!
				     */
				    return PM_ERR_CONV;
			    }
			    break;

		    }
		}
	    }
	    return np->info->numval;
	    break;

	case L_NAME:
	    /*
	     * Extract instance-values from pmResult and store them in
	     * ivlist[] as <int, pmAtomValue> pairs
	     */
	    for (j = 0; j < rp->numpmid; j++) {
		if (np->info->pmid == rp->vset[j]->pmid) {
		    free_ivlist(np);
		    np->info->numval = rp->vset[j]->numval;
		    if (np->info->numval <= 0)
			return np->info->numval;
		    if ((np->info->ivlist = (val_t *)malloc(np->info->numval*sizeof(val_t))) == NULL) {
			__pmNoMem("eval_expr: metric ivlist", np->info->numval*sizeof(val_t), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    for (i = 0; i < np->info->numval; i++) {
			np->info->ivlist[i].inst = rp->vset[j]->vlist[i].inst;
			switch (np->desc.type) {
			    case PM_TYPE_32:
			    case PM_TYPE_U32:
				np->info->ivlist[i].value.l = rp->vset[j]->vlist[i].value.lval;
				break;

			    case PM_TYPE_64:
			    case PM_TYPE_U64:
				memcpy((void *)&np->info->ivlist[i].value.ll, (void *)rp->vset[j]->vlist[i].value.pval->vbuf, sizeof(__int64_t));
				break;

			    case PM_TYPE_FLOAT:
				if (rp->vset[j]->valfmt == PM_VAL_INSITU) {
				    /* old style insitu float */
				    np->info->ivlist[i].value.l = rp->vset[j]->vlist[i].value.lval;
				}
				else {
				    assert(rp->vset[j]->vlist[i].value.pval->vtype == PM_TYPE_FLOAT);
				    memcpy((void *)&np->info->ivlist[i].value.f, (void *)rp->vset[j]->vlist[i].value.pval->vbuf, sizeof(float));
				}
				break;

			    case PM_TYPE_DOUBLE:
				memcpy((void *)&np->info->ivlist[i].value.d, (void *)rp->vset[j]->vlist[i].value.pval->vbuf, sizeof(double));
				break;

			    case PM_TYPE_STRING:
				need = rp->vset[j]->vlist[i].value.pval->vlen-PM_VAL_HDR_SIZE;
				if ((np->info->ivlist[i].value.cp = (char *)malloc(need)) == NULL) {
				    __pmNoMem("eval_expr: string value", rp->vset[j]->vlist[i].value.pval->vlen, PM_FATAL_ERR);
				    /*NOTREACHED*/
				}
				memcpy((void *)np->info->ivlist[i].value.cp, (void *)rp->vset[j]->vlist[i].value.pval->vbuf, need);
				np->info->ivlist[i].vlen = need;
				break;

			    case PM_TYPE_AGGREGATE:
			    case PM_TYPE_AGGREGATE_STATIC:
			    case PM_TYPE_EVENT:
			    case PM_TYPE_HIGHRES_EVENT:
				if ((np->info->ivlist[i].value.vbp = (pmValueBlock *)malloc(rp->vset[j]->vlist[i].value.pval->vlen)) == NULL) {
				    __pmNoMem("eval_expr: aggregate value", rp->vset[j]->vlist[i].value.pval->vlen, PM_FATAL_ERR);
				    /*NOTREACHED*/
				}
				memcpy(np->info->ivlist[i].value.vbp, (void *)rp->vset[j]->vlist[i].value.pval, rp->vset[j]->vlist[i].value.pval->vlen);
				np->info->ivlist[i].vlen = rp->vset[j]->vlist[i].value.pval->vlen;
				break;

			    default:
				/*
				 * really only PM_TYPE_NOSUPPORT should
				 * end up here
				 */
				return PM_ERR_TYPE;
			}
		    }
		    return np->info->numval;
		}
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_DERIVE) {
		char	strbuf[20];
		fprintf(stderr, "eval_expr: botch: operand %s not in the extended pmResult\n", pmIDStr_r(np->info->pmid, strbuf, sizeof(strbuf)));
		__pmDumpResult(stderr, rp);
	    }
#endif
	    return PM_ERR_PMID;

	case L_ANON:
	    /* no values available for anonymous metrics */
	    return 0;

	default:
	    /*
	     * binary operator cases ... always have a left and right
	     * operand and no errors (these are caught earlier when the
	     * recursive call on each of the operands would may have
	     * returned an error
	     */
	    assert(np->left != NULL);
	    assert(np->right != NULL);

	    free_ivlist(np);
	    /*
	     * empty result cases first
	     */
	    if (np->left->info->numval == 0) {
		np->info->numval = 0;
		return np->info->numval;
	    }
	    if (np->right->info->numval == 0) {
		np->info->numval = 0;
		return np->info->numval;
	    }
	    /*
	     * really got some work to do ...
	     */
	    if (np->left->desc.indom == PM_INDOM_NULL)
		np->info->numval = np->right->info->numval;
	    else if (np->right->desc.indom == PM_INDOM_NULL)
		np->info->numval = np->left->info->numval;
	    else {
		/*
		 * Generally have the same number of instances because
		 * both operands are over the same instance domain,
		 * fetched with the same profile.  When not the case,
		 * the result can contain no more instances than in
		 * the smaller of the operands.
		 */
		if (np->left->info->numval <= np->right->info->numval)
		    np->info->numval = np->left->info->numval;
		else
		    np->info->numval = np->right->info->numval;
	    }
	    if ((np->info->ivlist = (val_t *)malloc(np->info->numval*sizeof(val_t))) == NULL) {
		__pmNoMem("eval_expr: expr ivlist", np->info->numval*sizeof(val_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    /*
	     * ivlist[k] = left-ivlist[i] <op> right-ivlist[j]
	     */
	    for (i = j = k = 0; k < np->info->numval; ) {
		if (i >= np->left->info->numval || j >= np->right->info->numval) {
		    /* run out of operand instances, quit */
		    np->info->numval = k;
		    break;
		}
		if (np->left->desc.indom != PM_INDOM_NULL &&
		    np->right->desc.indom != PM_INDOM_NULL) {
		    if (np->left->info->ivlist[i].inst != np->right->info->ivlist[j].inst) {
			/* left ith inst != right jth inst ... search in right */
#ifdef PCP_DEBUG
			if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL2)) {
			    fprintf(stderr, "eval_expr: inst[%d] mismatch left [%d]=%d right [%d]=%d\n", k, i, np->left->info->ivlist[i].inst, j, np->right->info->ivlist[j].inst);
			}
#endif
			for (j = 0; j < np->right->info->numval; j++) {
			    if (np->left->info->ivlist[i].inst == np->right->info->ivlist[j].inst)
				break;
			}
			if (j == np->right->info->numval) {
			    /*
			     * no match, so next instance on left operand,
			     * and reset to start from first instance of
			     * right operand
			     */
			    i++;
			    j = 0;
			    continue;
			}
#ifdef PCP_DEBUG
			else {
			    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL2)) {
				fprintf(stderr, "eval_expr: recover @ right [%d]=%d\n", j, np->right->info->ivlist[j].inst);
			    }
			}
#endif
		    }
		}
		np->info->ivlist[k].value =
		    bin_op(np->desc.type, np->type,
			   np->left->info->ivlist[i].value, np->left->desc.type, np->left->info->mul_scale, np->left->info->div_scale,
			   np->right->info->ivlist[j].value, np->right->desc.type, np->right->info->mul_scale, np->right->info->div_scale);
		if (np->left->desc.indom != PM_INDOM_NULL)
		    np->info->ivlist[k].inst = np->left->info->ivlist[i].inst;
		else
		    np->info->ivlist[k].inst = np->right->info->ivlist[j].inst;
		k++;
		if (np->left->desc.indom != PM_INDOM_NULL) {
		    i++;
		    if (np->right->desc.indom != PM_INDOM_NULL) {
			j++;
			if (j >= np->right->info->numval) {
			    /* rescan if need be */
			    j = 0;
			}
		    }
		}
		else if (np->right->desc.indom != PM_INDOM_NULL) {
		    j++;
		}
	    }
	    return np->info->numval;
    }
    /*NOTREACHED*/
}

/*
 * Algorithm here is complicated by trying to re-write the pmResult.
 *
 * On entry the pmResult is likely to be built over a pinned PDU buffer,
 * which means individual pmValueSets cannot be selectively replaced
 * (this would come to tears badly in pmFreeResult() where as soon as
 * one pmValueSet is found to be in a pinned PDU buffer it is assumed
 * they are all so ... leaving a memory leak for any ones we'd modified
 * here).
 *
 * So the only option is to COPY the pmResult, selectively replacing
 * the pmValueSets for the derived metrics, and then calling
 * pmFreeResult() to free the input structure and return the new one.
 *
 * In making the COPY it is critical that we reverse the algorithm
 * used in pmFreeResult() so that a later call to pmFreeResult() will
 * not cause a memory leak.
 * This means ...
 * - malloc() the pmResult (padded out to the right number of vset[]
 *   entries)
 * - if valfmt is not PM_VAL_INSITU use PM_VAL_DPTR (not PM_VAL_SPTR),
 *   so anything we point to is going to be released when our caller
 *   calls pmFreeResult()
 * - use one malloc() for each pmValueSet with vlist[] sized to be 0
 *   if numval < 0 else numval
 * - pmValueBlocks are from malloc()
 *
 * For reference, the same logic appears in __pmLogFetchInterp() to
 * sythesize a pmResult there.
 */
void
__dmpostfetch(__pmContext *ctxp, pmResult **result)
{
    int		i;
    int		j;
    int		m;
    int		numval;
    int		valfmt;
    size_t	need;
    int		rewrite;
    ctl_t	*cp = (ctl_t *)ctxp->c_dm;
    pmResult	*rp = *result;
    pmResult	*newrp;

    /* if needed, init() called in __dmopencontext beforehand */

    if (cp == NULL || cp->fetch_has_dm == 0) return;

    newrp = (pmResult *)malloc(sizeof(pmResult)+(cp->numpmid-1)*sizeof(pmValueSet *));
    if (newrp == NULL) {
	__pmNoMem("__dmpostfetch: newrp", sizeof(pmResult)+(cp->numpmid-1)*sizeof(pmValueSet *), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    newrp->timestamp = rp->timestamp;
    newrp->numpmid = cp->numpmid;

    for (j = 0; j < newrp->numpmid; j++) {
	numval = rp->vset[j]->numval;
	valfmt = rp->vset[j]->valfmt;
	rewrite = 0;
	/*
	 * pandering to gcc ... m is not used unless rewrite == 1 in
	 * which case m is well-defined
	 */
	m = 0;
	if (pmid_domain(rp->vset[j]->pmid) == DYNAMIC_PMID &&
	    pmid_item(rp->vset[j]->pmid) != 0) {
	    for (m = 0; m < cp->nmetric; m++) {
		if (rp->vset[j]->pmid == cp->mlist[m].pmid) {
		    if (cp->mlist[m].expr == NULL) {
			numval = PM_ERR_PMID;
		    }
		    else {
			rewrite = 1;
			if (cp->mlist[m].expr->desc.type == PM_TYPE_32 ||
			    cp->mlist[m].expr->desc.type == PM_TYPE_U32)
			    valfmt = PM_VAL_INSITU;
			else
			    valfmt = PM_VAL_DPTR;
			numval = eval_expr(cp->mlist[m].expr, rp, 1);
#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL2)) {
	int	k;
	char	strbuf[20];

	fprintf(stderr, "__dmpostfetch: [%d] root node %s: numval=%d", j, pmIDStr_r(rp->vset[j]->pmid, strbuf, sizeof(strbuf)), numval);
	for (k = 0; k < numval; k++) {
	    fprintf(stderr, " vset[%d]: inst=%d", k, cp->mlist[m].expr->info->ivlist[k].inst);
	    if (cp->mlist[m].expr->desc.type == PM_TYPE_32)
		fprintf(stderr, " l=%d", cp->mlist[m].expr->info->ivlist[k].value.l);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_U32)
		fprintf(stderr, " u=%u", cp->mlist[m].expr->info->ivlist[k].value.ul);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_64)
		fprintf(stderr, " ll=%"PRIi64, cp->mlist[m].expr->info->ivlist[k].value.ll);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_U64)
		fprintf(stderr, " ul=%"PRIu64, cp->mlist[m].expr->info->ivlist[k].value.ull);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_FLOAT)
		fprintf(stderr, " f=%f", (double)cp->mlist[m].expr->info->ivlist[k].value.f);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_DOUBLE)
		fprintf(stderr, " d=%f", cp->mlist[m].expr->info->ivlist[k].value.d);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_STRING) {
		fprintf(stderr, " cp=%s (len=%d)", cp->mlist[m].expr->info->ivlist[k].value.cp, cp->mlist[m].expr->info->ivlist[k].vlen);
	    }
	    else {
		fprintf(stderr, " vbp=" PRINTF_P_PFX "%p (len=%d)", cp->mlist[m].expr->info->ivlist[k].value.vbp, cp->mlist[m].expr->info->ivlist[k].vlen);
	    }
	}
	fputc('\n', stderr);
	if (cp->mlist[m].expr->info != NULL)
	    __dmdumpexpr(cp->mlist[m].expr, 1);
    }
#endif
		    }
		    break;
		}
	    }
	}

	if (numval <= 0) {
	    /* only need pmid and numval */
	    need = sizeof(pmValueSet) - sizeof(pmValue);
	}
	else {
	    /* already one pmValue in a pmValueSet */
	    need = sizeof(pmValueSet) + (numval - 1)*sizeof(pmValue);
	}
	if (need > 0) {
	    if ((newrp->vset[j] = (pmValueSet *)malloc(need)) == NULL) {
		__pmNoMem("__dmpostfetch: vset", need, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	}
	newrp->vset[j]->pmid = rp->vset[j]->pmid;
	newrp->vset[j]->numval = numval;
	newrp->vset[j]->valfmt = valfmt;
	if (numval < 0)
	    continue;

	for (i = 0; i < numval; i++) {
	    pmValueBlock	*vp;

	    if (!rewrite) {
		newrp->vset[j]->vlist[i].inst = rp->vset[j]->vlist[i].inst;
		if (newrp->vset[j]->valfmt == PM_VAL_INSITU) {
		    newrp->vset[j]->vlist[i].value.lval = rp->vset[j]->vlist[i].value.lval;
		}
		else {
		    need = rp->vset[j]->vlist[i].value.pval->vlen;
		    vp = (pmValueBlock *)malloc(need);
		    if (vp == NULL) {
			__pmNoMem("__dmpostfetch: copy value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    memcpy((void *)vp, (void *)rp->vset[j]->vlist[i].value.pval, need);
		    newrp->vset[j]->vlist[i].value.pval = vp;
		}
		continue;
	    }

	    /*
	     * the rewrite case ...
	     */
	    newrp->vset[j]->vlist[i].inst = cp->mlist[m].expr->info->ivlist[i].inst;
	    switch (cp->mlist[m].expr->desc.type) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    newrp->vset[j]->vlist[i].value.lval = cp->mlist[m].expr->info->ivlist[i].value.l;
		    break;

		case PM_TYPE_64:
		case PM_TYPE_U64:
		    need = PM_VAL_HDR_SIZE + sizeof(__int64_t);
		    if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			__pmNoMem("__dmpostfetch: 64-bit int value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = cp->mlist[m].expr->desc.type;
		    memcpy((void *)vp->vbuf, (void *)&cp->mlist[m].expr->info->ivlist[i].value.ll, sizeof(__int64_t));
		    newrp->vset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_FLOAT:
		    need = PM_VAL_HDR_SIZE + sizeof(float);
		    if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			__pmNoMem("__dmpostfetch: float value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = PM_TYPE_FLOAT;
		    memcpy((void *)vp->vbuf, (void *)&cp->mlist[m].expr->info->ivlist[i].value.f, sizeof(float));
		    newrp->vset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_DOUBLE:
		    need = PM_VAL_HDR_SIZE + sizeof(double);
		    if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			__pmNoMem("__dmpostfetch: double value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = PM_TYPE_DOUBLE;
		    memcpy((void *)vp->vbuf, (void *)&cp->mlist[m].expr->info->ivlist[i].value.f, sizeof(double));
		    newrp->vset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_STRING:
		    need = PM_VAL_HDR_SIZE + cp->mlist[m].expr->info->ivlist[i].vlen;
		    vp = (pmValueBlock *)malloc(need);
		    if (vp == NULL) {
			__pmNoMem("__dmpostfetch: string value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = cp->mlist[m].expr->desc.type;
		    memcpy((void *)vp->vbuf, cp->mlist[m].expr->info->ivlist[i].value.cp, cp->mlist[m].expr->info->ivlist[i].vlen);
		    newrp->vset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_AGGREGATE:
		case PM_TYPE_AGGREGATE_STATIC:
		case PM_TYPE_EVENT:
		case PM_TYPE_HIGHRES_EVENT:
		    need = cp->mlist[m].expr->info->ivlist[i].vlen;
		    vp = (pmValueBlock *)malloc(need);
		    if (vp == NULL) {
			__pmNoMem("__dmpostfetch: aggregate or event value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    memcpy((void *)vp, cp->mlist[m].expr->info->ivlist[i].value.vbp, cp->mlist[m].expr->info->ivlist[i].vlen);
		    newrp->vset[j]->vlist[i].value.pval = vp;
		    break;

		default:
		    /*
		     * really nothing should end up here ...
		     * do nothing as numval should have been < 0
		     */
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_DERIVE) {
			char	strbuf[20];
			fprintf(stderr, "__dmpostfetch: botch: drived metric[%d]: operand %s has odd type (%d)\n", m, pmIDStr_r(rp->vset[j]->pmid, strbuf, sizeof(strbuf)), cp->mlist[m].expr->desc.type);
		    }
#endif
		    break;
	    }
	}
    }

    /*
     * cull the original pmResult and return the rewritten one
     */
    pmFreeResult(rp);
    *result = newrp;

    return;
}
