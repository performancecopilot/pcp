/*
 * Copyright (c) 2009 Ken McDonell.  All Rights Reserved.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 *
 * Debug Flags
 *	DERIVE - high-level diagnostics
 *	DERIVE & APPL0 - configuration and static syntax analysis
 *	DERIVE & APPL1 - expression binding and semantic analysis
 *	DERIVE & APPL2 - fetch handling
 */

#include "derive.h"

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
 * Return the number of pmIDs in the combined list.
 *
 * The derived metric pmIDs are left in the combined list (they will
 * return PM_ERR_NOAGENT from the fetch) to simplify the post-processing
 * of the pmResult in __dmpostfetch()
 */
int
__dmprefetch(__pmContext *ctxp, int numpmid, pmID *pmidlist, pmID **newlist)
{
    int		i;
    int		j;
    int		m;
    int		xtracnt = 0;
    pmID	*xtralist = NULL;
    pmID	*list;
    ctl_t	*cp = (ctl_t *)ctxp->c_dm;

    /* if needed, init() called in __dmopencontext beforehand */

    if (cp == NULL) return numpmid;

    /*
     * save numpmid to be used in __dmpostfetch() ... works because calls
     * to pmFetch cannot be nested (at all, but certainly for the same
     * context).
     * Ditto for the fast path flag (fetch_has_dm).
     */
    cp->numpmid = numpmid;
    cp->fetch_has_dm = 0;

    for (m = 0; m < numpmid; m++) {
	if (pmid_domain(pmidlist[m]) != DYNAMIC_PMID)
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
    if (xtracnt == 0) return numpmid;

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
    if (xtracnt == 0) return numpmid;

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL2)) {
	fprintf(stderr, "derived metrics prefetch added %d metrics:", xtracnt);
	for (i = 0; i < xtracnt; i++)
	    fprintf(stderr, " %s", pmIDStr(xtralist[i]));
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
 * the pmAtomValues may have buffers attached in the type STRING and
 * type AGGREGATE* cases.
 * Includes logic to save one history sample (for delta()).
 */
static void
free_ivlist(node_t *np)
{
    int		i;

    assert(np->info != NULL);

    if (np->save_last) {
	if (np->info->last_ivlist != NULL) {
	    if (np->desc.type == PM_TYPE_STRING ||
		np->desc.type == PM_TYPE_AGGREGATE ||
		np->desc.type == PM_TYPE_AGGREGATE_STATIC) {
		for (i = 0; i < np->info->last_numval; i++) {
		    if (np->info->last_ivlist[i].value.vp != NULL)
			free(np->info->last_ivlist[i].value.vp);
		}
	    }
	    free(np->info->last_ivlist);
	}
	np->info->last_numval = np->info->numval;
	if (np->info->iv_alloc == 0) {
	    /*
	     * saved last values has to be copied to avoid clobbering
	     * at next fetch
	     */
	    if ((np->info->last_ivlist = (val_t *)malloc(np->info->last_numval*sizeof(val_t))) == NULL) {
		__pmNoMem("free_ivlist: last ivlist", np->info->last_numval*sizeof(val_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    for (i = 0; i < np->info->last_numval; i++) {
		np->info->last_ivlist[i].inst = np->info->ivlist[i].inst;
		np->info->last_ivlist[i].vlen = np->info->ivlist[i].vlen;
		if (np->desc.type == PM_TYPE_STRING ||
		    np->desc.type == PM_TYPE_AGGREGATE ||
		    np->desc.type == PM_TYPE_AGGREGATE_STATIC) {
		    if ((np->info->last_ivlist[i].value.vp = (void *)malloc(np->info->last_ivlist[i].vlen)) == NULL) {
			__pmNoMem("free_ivlist: last value", np->info->last_ivlist[i].vlen, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    memcpy(np->info->last_ivlist[i].value.vp, np->info->ivlist[i].value.vp, np->info->last_ivlist[i].vlen);
		}
		else
		    memcpy((void *)&np->info->last_ivlist[i].value, (void *)&np->info->ivlist[i].value, sizeof(np->info->last_ivlist[0].value));
	    }
	}
	else {
	    np->info->last_ivlist = np->info->ivlist;
	}
    }
    else {
	/* no history */
	if (np->info->iv_alloc == 1)  {
	    if (np->info->ivlist != NULL) {
		if (np->desc.type == PM_TYPE_STRING ||
		    np->desc.type == PM_TYPE_AGGREGATE ||
		    np->desc.type == PM_TYPE_AGGREGATE_STATIC) {
		    for (i = 0; i < np->info->numval; i++) {
			if (np->info->ivlist[i].value.vp != NULL)
			    free(np->info->ivlist[i].value.vp);
		    }
		}
	    }
	    free(np->info->ivlist);
	}
	np->info->ivlist = NULL;
	np->info->numval = 0;
    }
}

/*
 * Binary arithmetic.
 */
static pmAtomValue
bin_op(int type, int op, pmAtomValue a, int ltype, pmAtomValue b, int rtype)
{
    static pmAtomValue	res;
    pmAtomValue		l;
    pmAtomValue		r;

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
		default:
		    assert(ltype == -100);	/* botch, so always true! */
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
		default:
		    assert(rtype == -100);	/* botch, so always true! */
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
		default:
		    assert(ltype == -100);	/* botch, so always true! */
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
		default:
		    assert(rtype == -100);	/* botch, so always true! */
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
		default:
		    assert(ltype == -100);	/* botch, so always true! */
	    }
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
		default:
		    assert(rtype == -100);	/* botch, so always true! */
	    }
	    break;
	default:
	    assert(type == -100);	/* botch, so always true! */
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
		case L_SLASH:
		    res.f = l.f / r.f;
		    break;
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
		    res.d = l.d / r.d;
		    break;
	    }
	    break;
	default:
	    assert(type == -100);	/* botch, so always true! */
    }

    return res;
}


/*
 * Walk an expression tree, filling in operand values from the
 * pmResult at the leaf nodes and propagating the computed values
 * towards the root node of the tree.
 *
 * The control variable iv_alloc determines if the ivlist[] entries
 * are allocated with the current node, or the current node points
 * to ivlist[] entries allocated in another node.
 */
static int
eval_expr(node_t *np, pmResult *rp, int level)
{
    int		sts;
    int		i;
    int		j;
    int		k;
    size_t	need;
    node_t	*src;

    assert(np != NULL);
    if (np->left != NULL) {
	sts = eval_expr(np->left, rp, level+1);
	if (sts <= 0) return sts;
    }
    if (np->right != NULL) {
	sts = eval_expr(np->right, rp, level+1);
	if (sts <= 0) return sts;
    }

    switch (np->type) {

	case L_NUMBER:
	    if (np->info->numval == 0) {
		/* initialize ivlist[] for singular instance first time through */
		np->info->numval = 1;
		if ((np->info->ivlist = (val_t *)malloc(sizeof(val_t))) == NULL) {
		    __pmNoMem("eval_expr: number ivlist", sizeof(val_t), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		np->info->iv_alloc = 1;
		np->info->ivlist[0].inst = PM_INDOM_NULL;
		/* don't need error checking, done in the lexical scanner */
		np->info->ivlist[0].value.l = atoi(np->value);
	    }
	    return 1;

	case L_DELTA:
	    /*
	     * this and the last values are in the left expr
	     */
	    free_ivlist(np);
	    np->info->numval = np->left->info->numval <= np->left->info->last_numval ? np->left->info->numval : np->left->info->last_numval;
	    if (np->info->numval <= 0)
		return np->info->numval;
	    if ((np->info->ivlist = (val_t *)malloc(np->info->numval*sizeof(val_t))) == NULL) {
		__pmNoMem("eval_expr: delta() ivlist", np->info->numval*sizeof(val_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    np->info->iv_alloc = 1;
	    for (i = k = 0; k < np->info->numval; i++) {
		if (i >= np->left->info->numval) {
		    /* run out of current instances */
		    np->info->numval = k;
		    break;
		}
		j = i;
		if (j >= np->left->info->last_numval ||
		    np->left->info->ivlist[i].inst != np->left->info->last_ivlist[j].inst) {
		    /* current ith inst != last jth inst ... search in last */
		    for (j = 0; j < np->left->info->last_numval; j++) {
			if (np->left->info->ivlist[i].inst == np->left->info->last_ivlist[j].inst)
			    break;
		    }
		    if (j == np->left->info->last_numval) {
			/* no match, one less result instance */
			np->info->numval--;
			continue;
		    }
		}
		np->info->ivlist[k].inst = np->left->info->ivlist[i].inst;
		switch (np->desc.type) {
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
		k++;
	    }
	    return np->info->numval;

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
		    np->info->iv_alloc = 1;
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
				if ((np->info->ivlist[i].value.vp = (void *)malloc(rp->vset[j]->vlist[i].value.pval->vlen)) == NULL) {
				    __pmNoMem("eval_expr: aggregate value", rp->vset[j]->vlist[i].value.pval->vlen, PM_FATAL_ERR);
				    /*NOTREACHED*/
				}
				memcpy(np->info->ivlist[i].value.vp, (void *)rp->vset[j]->vlist[i].value.pval->vbuf, rp->vset[j]->vlist[i].value.pval->vlen-PM_VAL_HDR_SIZE);
				np->info->ivlist[i].vlen = rp->vset[j]->vlist[i].value.pval->vlen-PM_VAL_HDR_SIZE;
				break;

			    default:
				/*
				 * really only PM_TYPE_NOSUPPORT should
				 * end up here
				 */
				return PM_ERR_APPVERSION;
			}
		    }
		    return np->info->numval;
		}
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_DERIVE) {
		fprintf(stderr, "eval_expr: botch: operand %s not in the extended pmResult\n", pmIDStr(np->info->pmid));
		__pmDumpResult(stderr, rp);
	    }
#endif
	    return PM_ERR_PMID;


	default:
	    src = NULL;
	    if (np->left == NULL) {
		/* copy right pmValues */
		src = np->right;
	    }
	    else if (np->right == NULL) {
		/* copy left pmValues */
		src = np->left;
	    }
	    if (src != NULL) {
		np->info->numval = src->info->numval;
		np->info->iv_alloc = 0;
		np->info->ivlist = src->info->ivlist;
		return np->info->numval;
	    }
	    else {
		free_ivlist(np);
		/*
		 * Error cases first ... if both are in error, arbitrarily
		 * choose the left error code.
		 * Then the empty result cases.
		 */
		if (np->left->info->numval < 0) {
		    np->info->numval = np->left->info->numval;
		    return np->info->numval;
		}
		if (np->right->info->numval < 0) {
		    np->info->numval = np->right->info->numval;
		    return np->info->numval;
		}
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
		     * Should have the same number of instances because
		     * both operands are over the same instance domain,
		     * fetched with the same profile.
		     */
		    assert(np->left->info->numval == np->right->info->numval);
		    np->info->numval = np->left->info->numval;
		}
		if ((np->info->ivlist = (val_t *)malloc(np->info->numval*sizeof(val_t))) == NULL) {
		    __pmNoMem("eval_expr: expr ivlist", np->info->numval*sizeof(val_t), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		np->info->iv_alloc = 1;
		/*
		 * ivlist[k] = left-ivlist[i] <op> right-ivlist[j]
		 *
		 * TODO - not quit right ... the zero-trip case may pick
		 * different instances
		 */
		for (i = j = k = 0; ; k++) {
		    np->info->ivlist[k].value =
			bin_op(np->desc.type, np->type,
			       np->left->info->ivlist[i].value, np->left->desc.type,
			       np->right->info->ivlist[j].value, np->right->desc.type);
		    if (np->left->desc.indom != PM_INDOM_NULL)
			np->info->ivlist[k].inst = np->left->info->ivlist[i].inst;
		    else
			np->info->ivlist[k].inst = np->right->info->ivlist[j].inst;
		    if (k == np->info->numval - 1)
			break;
		    if (np->left->desc.indom != PM_INDOM_NULL) {
			i++;
			if (np->right->desc.indom != PM_INDOM_NULL) {
			    j++;
			    if (np->left->info->ivlist[i].inst == np->right->info->ivlist[j].inst)
				continue;
			    /*
			     * Should not happen ... both metrics are over
			     * the same instance domain, fetched with the
			     * same profile, so it is reasonable to assume
			     * instances will be returned in the same order
			     * for both metrics ... if it does happen, skip
			     * this instance, but there is likely to be a
			     * "knock on" effect with the following instances
			     * for this expression failing this same test
			     */
#ifdef PCP_DEBUG
			    if (pmDebug & DBG_TRACE_DERIVE) {
				fprintf(stderr, "eval_expr: %s inst mismatch left [%d]=%d right [%d]=%d\n", pmIDStr(np->info->pmid), i, np->left->info->ivlist[i].inst, j, np->right->info->ivlist[j].inst);
			    }
#endif
			    np->info->numval--;
			    k--;
			}
		    }
		    else if (np->right->desc.indom != PM_INDOM_NULL) {
			j++;
		    }
		}
		return np->info->numval;
	    }
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
 * - if numval == 1,  use __pmPoolAlloc() for the pmValueSet;
 *   otherwise use one malloc() for each pmValueSet with vlist[] sized
 *   to be 0 if numval < 0 else numval
 * - pmValueBlocks for 64-bit integers, doubles or anything with a
 *   length equal to the size of a 64-bit integer are from
 *   __pmPoolAlloc(); otherwise pmValueBlocks are from malloc()
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
	if (pmid_domain(rp->vset[j]->pmid) == DYNAMIC_PMID) {
	    for (m = 0; m < cp->nmetric; m++) {
		if (rp->vset[j]->pmid == cp->mlist[m].pmid) {
		    rewrite = 1;
		    if (cp->mlist[m].expr->desc.type == PM_TYPE_32 ||
			cp->mlist[m].expr->desc.type == PM_TYPE_U32)
			valfmt = PM_VAL_INSITU;
		    else
			valfmt = PM_VAL_DPTR;
		    if (cp->mlist[m].expr == NULL) {
			numval = PM_ERR_PMID;
		    }
		    else {
			numval = eval_expr(cp->mlist[m].expr, rp, 1);
#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_DERIVE) && (pmDebug & DBG_TRACE_APPL2)) {
	int	k;

	fprintf(stderr, "__dmpostfetch: [%d] root node %s: numval=%d", j, pmIDStr(rp->vset[j]->pmid), numval);
	for (k = 0; k < numval; k++) {
	    fprintf(stderr, " vset[%d]: inst=%d", k, cp->mlist[m].expr->info->ivlist[k].inst);
	    if (cp->mlist[m].expr->desc.type == PM_TYPE_32)
		fprintf(stderr, " l=%d", cp->mlist[m].expr->info->ivlist[k].value.l);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_U32)
		fprintf(stderr, " u=%u", cp->mlist[m].expr->info->ivlist[k].value.ul);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_64)
		fprintf(stderr, " ll=%lld", cp->mlist[m].expr->info->ivlist[k].value.ll);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_U64)
		fprintf(stderr, " ul=%llu", cp->mlist[m].expr->info->ivlist[k].value.ull);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_FLOAT)
		fprintf(stderr, " f=%f", (double)cp->mlist[m].expr->info->ivlist[k].value.f);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_DOUBLE)
		fprintf(stderr, " d=%f", cp->mlist[m].expr->info->ivlist[k].value.f);
	    else if (cp->mlist[m].expr->desc.type == PM_TYPE_STRING) {
		fprintf(stderr, " cp=%s (len=%d)", cp->mlist[m].expr->info->ivlist[k].value.cp, cp->mlist[m].expr->info->ivlist[k].vlen);
	    }
	    else {
		fprintf(stderr, " vp=%p (len=%d)", cp->mlist[m].expr->info->ivlist[k].value.vp, cp->mlist[m].expr->info->ivlist[k].vlen);
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

	if (numval < 0) {
	    /* only need pmid and numval */
	    need = sizeof(pmValueSet) - sizeof(pmValue);
	}
	else if (numval == 1) {
	    /* special case for single value */
	    newrp->vset[j] = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
	    need = 0;
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
		    if (need == PM_VAL_HDR_SIZE + sizeof(__int64_t))
			vp = (pmValueBlock *)__pmPoolAlloc(need);
		    else
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
		    if ((vp = (pmValueBlock *)__pmPoolAlloc(need)) == NULL) {
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
		    if ((vp = (pmValueBlock *)__pmPoolAlloc(need)) == NULL) {
			__pmNoMem("__dmpostfetch: double value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = PM_TYPE_DOUBLE;
		    memcpy((void *)vp->vbuf, (void *)&cp->mlist[m].expr->info->ivlist[i].value.f, sizeof(double));
		    newrp->vset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_STRING:
		case PM_TYPE_AGGREGATE:
		case PM_TYPE_AGGREGATE_STATIC:
		    need = PM_VAL_HDR_SIZE + cp->mlist[m].expr->info->ivlist[i].vlen;
		    if (need == PM_VAL_HDR_SIZE + sizeof(__int64_t))
			vp = (pmValueBlock *)__pmPoolAlloc(need);
		    else
			vp = (pmValueBlock *)malloc(need);
		    if (vp == NULL) {
			__pmNoMem("__dmpostfetch: string or aggregate value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = cp->mlist[m].expr->desc.type;
		    memcpy((void *)vp->vbuf, cp->mlist[m].expr->info->ivlist[i].value.vp, cp->mlist[m].expr->info->ivlist[i].vlen);
		    newrp->vset[j]->vlist[i].value.pval = vp;
		    break;

		default:
		    /*
		     * really nothing should end up here ...
		     * do nothing as numval should have been < 0
		     */
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_DERIVE) {
			fprintf(stderr, "__dmpostfetch: botch: drived metric[%d]: operand %s has odd type (%d)\n", m, pmIDStr(rp->vset[j]->pmid), cp->mlist[m].expr->desc.type);
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
