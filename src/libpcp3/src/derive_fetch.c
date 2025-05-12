/*
 * Copyright (c) 2009,2014 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2021-2022 Red Hat.
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
#include "libpcp.h"
#include "internal.h"

extern const int promote[6][6];

/*
 * recursive descent to harvest names of valid metrics from
 * an expression tree
 */
static void
get_pmids(node_t *np, int *cnt, pmID **list)
{
    assert(np != NULL);
    if (np->type == N_NOVALUE || np->type == N_INTEGER || np->type == N_DOUBLE)
	return;
    if (np->left != NULL) get_pmids(np->left, cnt, list);
    if (np->right != NULL) get_pmids(np->right, cnt, list);
    if (np->type == N_NAME && np->data.info->pmid != PM_ID_NULL) {
	(*cnt)++;
	if ((*list = (pmID *)realloc(*list, (*cnt)*sizeof(pmID))) == NULL) {
	    pmNoMem("get_pmids: realloc xtralist", (*cnt)*sizeof(pmID), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	(*list)[*cnt-1] = np->data.info->pmid;
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

    /* if needed, __dminit() called in __dmopencontext beforehand */

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
	if (!IS_DERIVED(pmidlist[m]))
	    continue;
	for (i = 0; i < cp->nmetric; i++) {
	    if (pmidlist[m] == cp->mlist[i].pmid) {
		if ((cp->mlist[i].flags & DM_BIND) == 0)
		    __dmbind(PM_NOT_LOCKED, ctxp, i, 1);
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

    if (pmDebugOptions.derive && pmDebugOptions.appl2) {
	char	strbuf[20];
	fprintf(stderr, "derived metrics prefetch added %d metrics:", xtracnt);
	for (i = 0; i < xtracnt; i++)
	    fprintf(stderr, " %s", pmIDStr_r(xtralist[i], strbuf, sizeof(strbuf)));
	fputc('\n', stderr);
    }
    if ((list = (pmID *)malloc((numpmid+xtracnt)*sizeof(pmID))) == NULL) {
	pmNoMem("__dmprefetch: alloc list", (numpmid+xtracnt)*sizeof(pmID), PM_FATAL_ERR);
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
* saving history for delta() or rate() ... release previous sample
* last_ivlist[] and save this sample last_ivlist[] <- ivlist[]
*/
static void
save_ivlist(node_t *np)
{
    if (np->save_last) {
	if (np->type == N_INTEGER || np->type == N_DOUBLE) {
	    /*
	     * these will never change, so fake out the "last"
	     * ones the first time through
	     */
	    if (np->data.info->ivlist != NULL &&
	        np->data.info->last_ivlist == NULL) {
		np->data.info->last_numval = np->data.info->numval;
		np->data.info->last_ivlist = np->data.info->ivlist;
	    }
	}
	else {
	    if (np->data.info->last_ivlist != NULL) {
		/*
		 * no STRING, AGGREGATE or EVENT types for delta() or rate()
		 * so simple free()
		 */
		free(np->data.info->last_ivlist);
	    }
	    np->data.info->last_numval = np->data.info->numval;
	    np->data.info->last_ivlist = np->data.info->ivlist;
	    np->data.info->ivlist = NULL;
	}
    }
}

/*
 * Either call save_ivlist() to free last_ivlist[] and save one history
 * sample (for delta() and rate()), or free ivlist[] (if any) ... may
 * need to walk the list because the pmAtomValues may have buffers attached
 * in the type STRING, type AGGREGATE* and type EVENT cases.
 */
static void
free_ivlist(node_t *np)
{
    int		i;

    assert(np->data.info != NULL);

    if (np->save_last)
	save_ivlist(np);
    else {
	/* no history */
	if (np->data.info->ivlist != NULL) {
	    if (np->desc.type == PM_TYPE_STRING) {
		for (i = 0; i < np->data.info->numval; i++) {
		    if (np->data.info->ivlist[i].value.cp != NULL)
			free(np->data.info->ivlist[i].value.cp);
		}
	    }
	    else if (np->desc.type == PM_TYPE_AGGREGATE ||
		     np->desc.type == PM_TYPE_AGGREGATE_STATIC ||
		     np->desc.type == PM_TYPE_EVENT ||
		     np->desc.type == PM_TYPE_HIGHRES_EVENT) {
		for (i = 0; i < np->data.info->numval; i++) {
		    if (np->data.info->ivlist[i].value.vbp != NULL)
			free(np->data.info->ivlist[i].value.vbp);
		}
	    }
	}
	free(np->data.info->ivlist);
	np->data.info->numval = 0;
	np->data.info->ivlist = NULL;
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
     * Do the operators ... messy!
     */
    switch (type) {
	case PM_TYPE_32:
	    switch (op) {
		case N_PLUS:
		    res.l = l.l + r.l;
		    break;
		case N_MINUS:
		    res.l = l.l - r.l;
		    break;
		case N_STAR:
		    res.l = l.l * r.l;
		    break;
		/* semantics enforce no N_SLASH for integer results */
		case N_LT:
		    res.l = l.l < r.l;
		    break;
		case N_LEQ:
		    res.l = l.l <= r.l;
		    break;
		case N_EQ:
		    res.l = l.l == r.l;
		    break;
		case N_GEQ:
		    res.l = l.l >= r.l;
		    break;
		case N_GT:
		    res.l = l.l > r.l;
		    break;
		case N_NEQ:
		    res.l = l.l != r.l;
		    break;
		case N_AND:
		    res.l = (l.l != 0) && (r.l != 0);
		    break;
		case N_OR:
		    res.l = (l.l != 0) || (r.l != 0);
		    break;
		default:	/* should not happen */
		    fprintf(stderr, "bin_op: botch: 32 op=%d\n", op);
		    res.l = 0;
		    break;
	    }
	    break;
	case PM_TYPE_U32:
	    switch (op) {
		case N_PLUS:
		    res.ul = l.ul + r.ul;
		    break;
		case N_MINUS:
		    res.ul = l.ul - r.ul;
		    break;
		case N_STAR:
		    res.ul = l.ul * r.ul;
		    break;
		/* semantics enforce no N_SLASH for integer results */
		case N_LT:
		    res.ul = l.ul < r.ul;
		    break;
		case N_LEQ:
		    res.ul = l.ul <= r.ul;
		    break;
		case N_EQ:
		    res.ul = l.ul == r.ul;
		    break;
		case N_GEQ:
		    res.ul = l.ul >= r.ul;
		    break;
		case N_GT:
		    res.ul = l.ul > r.ul;
		    break;
		case N_NEQ:
		    res.ul = (l.ul != r.ul);
		    break;
		case N_AND:
		    res.ul = (l.ul != 0) && (r.ul != 0);
		    break;
		case N_OR:
		    res.ul = (l.ul != 0) || (r.ul != 0);
		    break;
		default:	/* should not happen */
		    fprintf(stderr, "bin_op: botch: U32 op=%d\n", op);
		    res.ul = 0;
	    }
	    break;
	case PM_TYPE_64:
	    switch (op) {
		case N_PLUS:
		    res.ll = l.ll + r.ll;
		    break;
		case N_MINUS:
		    res.ll = l.ll - r.ll;
		    break;
		case N_STAR:
		    res.ll = l.ll * r.ll;
		    break;
		/* semantics enforce no N_SLASH for integer results */
		case N_LT:
		    res.ll = l.ll < r.ll;
		    break;
		case N_LEQ:
		    res.ll = l.ll <= r.ll;
		    break;
		case N_EQ:
		    res.ll = l.ll == r.ll;
		    break;
		case N_GEQ:
		    res.ll = l.ll >= r.ll;
		    break;
		case N_GT:
		    res.ll = l.ll > r.ll;
		    break;
		case N_NEQ:
		    res.ll = l.ll != r.ll;
		    break;
		case N_AND:
		    res.ll = (l.ll != 0) && (r.ll != 0);
		    break;
		case N_OR:
		    res.ll = (l.ll != 0) || (r.ll != 0);
		    break;
		default:	/* should not happen */
		    fprintf(stderr, "bin_op: botch: 64 op=%d\n", op);
		    res.ll = 0;
	    }
	    break;
	case PM_TYPE_U64:
	    switch (op) {
		case N_PLUS:
		    res.ull = l.ull + r.ull;
		    break;
		case N_MINUS:
		    res.ull = l.ull - r.ull;
		    break;
		case N_STAR:
		    res.ull = l.ull * r.ull;
		    break;
		/* semantics enforce no N_SLASH for integer results */
		case N_LT:
		    res.ull = l.ull < r.ull;
		    break;
		case N_LEQ:
		    res.ull = l.ull <= r.ull;
		    break;
		case N_EQ:
		    res.ull = l.ull == r.ull;
		    break;
		case N_GEQ:
		    res.ull = l.ull >= r.ull;
		    break;
		case N_GT:
		    res.ull = l.ull > r.ull;
		    break;
		case N_NEQ:
		    res.ull = l.ull != r.ull;
		    break;
		case N_AND:
		    res.ull = (l.ull != 0) && (r.ull != 0);
		    break;
		case N_OR:
		    res.ull = (l.ull != 0) || (r.ull != 0);
		    break;
		default:	/* should not happen */
		    fprintf(stderr, "bin_op: botch: U64 op=%d\n", op);
		    res.ull = 0;
	    }
	    break;
	case PM_TYPE_FLOAT:
	    switch (op) {
		case N_PLUS:
		    res.f = l.f + r.f;
		    break;
		case N_MINUS:
		    res.f = l.f - r.f;
		    break;
		case N_STAR:
		    res.f = l.f * r.f;
		    break;
		/* semantics enforce no N_SLASH for float results */
		case N_LT:
		    res.f = l.f < r.f;
		    break;
		case N_LEQ:
		    res.f = l.f <= r.f;
		    break;
		case N_EQ:
		    res.f = l.f == r.f;
		    break;
		case N_GEQ:
		    res.f = l.f >= r.f;
		    break;
		case N_GT:
		    res.f = l.f > r.f;
		    break;
		case N_NEQ:
		    res.f = l.f != r.f;
		    break;
		case N_AND:
		    res.f = (l.f != 0) && (r.f != 0);
		    break;
		case N_OR:
		    res.f = (l.f != 0) || (r.f != 0);
		    break;
		default:	/* should not happen */
		    fprintf(stderr, "bin_op: botch: FLOAT op=%d\n", op);
		    res.f = 0;
	    }
	    break;
	case PM_TYPE_DOUBLE:
	    switch (op) {
		case N_PLUS:
		    res.d = l.d + r.d;
		    break;
		case N_MINUS:
		    res.d = l.d - r.d;
		    break;
		case N_STAR:
		    res.d = l.d * r.d;
		    break;
		case N_SLASH:
		    if (l.d == 0)
			res.d = 0;
		    else
			res.d = l.d / r.d;
		    break;
		case N_LT:
		    res.d = l.d < r.d;
		    break;
		case N_LEQ:
		    res.d = l.d <= r.d;
		    break;
		case N_EQ:
		    res.d = l.d == r.d;
		    break;
		case N_GEQ:
		    res.d = l.d >= r.d;
		    break;
		case N_GT:
		    res.d = l.d > r.d;
		    break;
		case N_NEQ:
		    res.d = l.d != r.d;
		    break;
		case N_AND:
		    res.d = (l.d != 0) && (r.d != 0);
		    break;
		case N_OR:
		    res.d = (l.d != 0) || (r.d != 0);
		    break;
		default:	/* should not happen */
		    fprintf(stderr, "bin_op: botch: DOUBLE op=%d\n", op);
		    res.d = 0;
	    }
	    break;
	default:	/* should not happen, but coverity does not know that */
	    fprintf(stderr, "bin_op: botch: type=%d is invalid\n", type);
	    res.ll = 0;	/* not great, but the best we can do */
	    break;
    }

    return res;
}

/*
 * For regular expression instance matching, the hash list of observed
 * instances could grow without bounds for a dynamic indom.
 * Use a simple algorithm ... if the instance has been observed in
 * less than 50% of the the last REGEX_INST_COMPACT fetches for the
 * regular expression node, drop the instance.  If a culled instance
 * comes back again we will add it into the hash list again.
 */
static void
regex_inst_gc(pattern_t *pp)
{
    int			numinst = 0;
    int			numcull = 0;
    __pmHashNode	*hnp;
    instctl_t		*icp;
    if (pmDebugOptions.derive && pmDebugOptions.appl2)
	fprintf(stderr, "regex_inst_gc(" PRINTF_P_PFX "%p)", pp);
    for (hnp = __pmHashWalk(&pp->hash, PM_HASH_WALK_START);
	 hnp != NULL;
	 hnp = __pmHashWalk(&pp->hash, PM_HASH_WALK_NEXT)) {
	numinst++;
	icp = (instctl_t *)hnp->data;
	if (icp->used < REGEX_INST_COMPACT / 2) {
	    int		sts;
	    sts = __pmHashDel(icp->inst, hnp->data, &pp->hash);
	    if (sts < 0) {
		fprintf(stderr, "botch: __pmHashDel: failed for inst=%d\n", icp->inst);
	    }
	    free(icp);
	    numcull++;
	}
	else
	    icp->used = 0;
    }
    if (pmDebugOptions.derive && pmDebugOptions.appl2)
	fprintf(stderr, " %d instances scanned, %d instances culled\n", numinst, numcull);
    pp->used = 0;
}

/*
 * insert constant value into an element of ivlist[]
 */
static void
stuff_constant(node_t *np, int i)
{
    /*
     * don't need error checking, done in the lexical scanner
     * but with the advent of mkconst() the type may not be as
     * simple as PM_TYPE_U32 or PM_TYPE_DOUBLE
     */
    switch (np->desc.type) {
	case PM_TYPE_32:
	    np->data.info->ivlist[i].value.l = atoi(np->value);
	    break;
	case PM_TYPE_U32:
	    np->data.info->ivlist[i].value.ul = atoi(np->value);
	    break;
	case PM_TYPE_64:
	    np->data.info->ivlist[i].value.ll = strtoll(np->value, NULL, 10);
	    break;
	case PM_TYPE_U64:
	    np->data.info->ivlist[i].value.ll = strtoull(np->value, NULL, 10);
	    break;
	case PM_TYPE_FLOAT:
	    np->data.info->ivlist[i].value.f = atof(np->value);
	    break;
	case PM_TYPE_DOUBLE:
	    np->data.info->ivlist[i].value.d = atof(np->value);
	    break;
    }
}

/*
 * setup ivlist[] values for a constant value
 */
static void
adjust_constant(__pmContext *ctxp, node_t *np)
{
    if (np->desc.indom == PM_INDOM_NULL) {
	if (np->data.info->numval == 0) {
	    /* initialize ivlist[] for singular instance first time through */
	    np->data.info->numval = 1;
	    if ((np->data.info->ivlist = (val_t *)malloc(sizeof(val_t))) == NULL) {
		pmNoMem("adjust_constant: number ivlist singular", sizeof(val_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    stuff_constant(np, 0);
	    np->data.info->ivlist[0].inst = PM_INDOM_NULL;
	    /* and ivlist[i].inst set by caller */
	}
    }
    else {
	/* refresh indom ... */
	int	sts;
	int	i;
	int	*instlist;
	char	**namelist;

	sts = pmGetInDom_ctx(ctxp, np->desc.indom, &instlist, &namelist);
	if (np->data.info->ivlist != NULL && sts > 0 && sts == np->data.info->numval) {
	    /*
	     * not the first time and same number of instances as last
	     * time ... if instances (order and id) are identical, we're
	     * done
	     */
	    for (i = 0; i < sts; i++) {
		if (instlist[i] != np->data.info->ivlist[i].inst)
		    break;
	    }
	    if (i == sts) {
		free(instlist);
		free(namelist);
		return;
	    }
	}

	/* rebuild ... */
	if (np->data.info->ivlist != NULL) {
	    free(np->data.info->ivlist);
	    np->data.info->ivlist = NULL;
	}

	if (sts < 0) {
	    /* pmGetInDom failed, we're doomed ... */
	    np->data.info->numval = sts;
	    return;
	}
	if ((np->data.info->ivlist = (val_t *)malloc(sts * sizeof(val_t))) == NULL) {
	    pmNoMem("adjust_constant: number indom ivlist", sts * sizeof(val_t), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	np->data.info->numval = 0;
	for (i = 0; i < sts; i++) {
	    /* may need instance profile filtering ... */
	    if (ctxp->c_instprof != NULL) {
		if (!__pmInProfile(np->desc.indom, ctxp->c_instprof, instlist[i]))
		    continue;
	    }
	    stuff_constant(np, np->data.info->numval);
	    np->data.info->ivlist[np->data.info->numval].inst = instlist[i];
	    np->data.info->numval++;
	}
	free(instlist);
	free(namelist);
    }
}

/*
 * Walk an expression tree, filling in operand values from the
 * pmResult at the leaf nodes and propagating the computed values
 * towards the root node of the tree.
 */
static int
eval_expr(__pmContext *ctxp, node_t *np, struct timespec *stamp, int numpmid,
		pmValueSet **vset, int level)
{
    int		sts;
    int		i;
    int		j;
    int		k;
    size_t	need;
    char	strbuf[20];

    assert(np != NULL);
    if (np->left != NULL &&
	np->type != N_NOVALUE && np->type != N_INTEGER && np->type != N_DOUBLE &&
	(np->type != N_COLON || (np->data.info->bind & QUEST_BIND_LEFT))) {
	sts = eval_expr(ctxp, np->left, stamp, numpmid, vset, level+1);
	if (sts < 0) {
	    if (np->type == N_COUNT) {
		/* count() ... special case, map errors to 0 */
		if (np->data.info->ivlist == NULL) {
		    /* initialize ivlist[] for singular instance first time through */
		    if ((np->data.info->ivlist = (val_t *)malloc(sizeof(val_t))) == NULL) {
			pmNoMem("eval_expr: count ivlist", sizeof(val_t), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    np->data.info->ivlist[0].inst = PM_IN_NULL;
		}
		np->data.info->numval = 1;
		np->data.info->ivlist[0].value.l = 0;
		sts = 1;
	    }
	    return sts;
	}
    }
    if (np->right != NULL &&
        (np->type != N_COLON || (np->data.info->bind & QUEST_BIND_RIGHT))) {
	sts = eval_expr(ctxp, np->right, stamp, numpmid, vset, level+1);
	if (sts < 0) return sts;
    }

    /* mostly, np->left is not NULL ... */
    assert (np->type == N_INTEGER || np->type == N_DOUBLE ||
            np->type == N_NAME || np->type == N_SCALE ||
	    np->type == N_FILTERINST || np->type == N_PATTERN ||
	    np->type == N_DEFINED || np->type == N_NOVALUE ||
	    np->left != NULL);

    switch (np->type) {

	case N_INTEGER:
	case N_DOUBLE:
	    save_ivlist(np);
	    adjust_constant(ctxp, np);
	    return np->data.info->numval;

	case N_DELTA:
	case N_RATE:
	    /*
	     * this and the last values are in the left expr
	     */
	    np->data.info->last_stamp = np->data.info->stamp;
	    np->data.info->stamp = *stamp;
	    free_ivlist(np);
	    np->data.info->numval = np->left->data.info->numval <= np->left->data.info->last_numval ? np->left->data.info->numval : np->left->data.info->last_numval;
	    if (np->data.info->numval <= 0)
		return np->data.info->numval;
	    if ((np->data.info->ivlist = (val_t *)malloc(np->data.info->numval*sizeof(val_t))) == NULL) {
		pmNoMem("eval_expr: delta()/rate() ivlist", np->data.info->numval*sizeof(val_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    /*
	     * delta()
	     * ivlist[k] = left->ivlist[i] - left->last_ivlist[j]
	     * rate()
	     * ivlist[k] = (left->ivlist[i] - left->last_ivlist[j]) /
	     *             (timestamp - left->last_stamp)
	     */
	    for (i = k = 0; i < np->left->data.info->numval; i++) {
		j = i;
		if (j >= np->left->data.info->last_numval)
		    j = 0;
		if (np->left->data.info->ivlist[i].inst != np->left->data.info->last_ivlist[j].inst) {
		    /* current ith inst != last jth inst ... search in last */
		    if ((pmDebugOptions.derive && pmDebugOptions.appl2) &&
			np->left->type != N_FILTERINST) {
			fprintf(stderr, "eval_expr: %s: inst[%d] mismatch left [%d]=%d last [%d]=%d\n",
			    __dmnode_type_str(np->type), k,
			    i, np->left->data.info->ivlist[i].inst,
			    j, np->left->data.info->last_ivlist[j].inst);
		    }
		    for (j = 0; j < np->left->data.info->last_numval; j++) {
			if (np->left->data.info->ivlist[i].inst == np->left->data.info->last_ivlist[j].inst)
			    break;
		    }
		    if (j == np->left->data.info->last_numval) {
			/* no match, skip this instance from this result */
			continue;
		    }
		    else {
			if ((pmDebugOptions.derive && pmDebugOptions.appl2) &&
			    np->left->type != N_FILTERINST) {
			    fprintf(stderr, "eval_expr: recover @ last [%d]=%d\n", j, np->left->data.info->last_ivlist[j].inst);
			}
		    }
		}
		np->data.info->ivlist[k].inst = np->left->data.info->ivlist[i].inst;
		if (np->type == N_DELTA) {
		    /* for delta() result type == operand type */
		    switch (np->left->desc.type) {
			case PM_TYPE_32:
			    np->data.info->ivlist[k].value.l = np->left->data.info->ivlist[i].value.l - np->left->data.info->last_ivlist[j].value.l;
			    break;
			case PM_TYPE_U32:
			    /* result promoted to 64 by parser */
			    np->data.info->ivlist[k].value.ll = np->left->data.info->ivlist[i].value.ul;
			    np->data.info->ivlist[k].value.ll -= np->left->data.info->last_ivlist[j].value.ul;
			    break;
			case PM_TYPE_64:
			    np->data.info->ivlist[k].value.ll = np->left->data.info->ivlist[i].value.ll - np->left->data.info->last_ivlist[j].value.ll;
			    break;
			case PM_TYPE_U64:
			    /* result promoted to DOUBLE by parser */
			    np->data.info->ivlist[k].value.d = np->left->data.info->ivlist[i].value.ull;
			    np->data.info->ivlist[k].value.d -= np->left->data.info->last_ivlist[j].value.ull;
			    break;
			case PM_TYPE_FLOAT:
			    np->data.info->ivlist[k].value.f = np->left->data.info->ivlist[i].value.f - np->left->data.info->last_ivlist[j].value.f;
			    break;
			case PM_TYPE_DOUBLE:
			    np->data.info->ivlist[k].value.d = np->left->data.info->ivlist[i].value.d - np->left->data.info->last_ivlist[j].value.d;
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
		    /*
		     * rate() conversion, type will be DOUBLE
		     *
		     * For COUNTER metrics, return "no value" if the counter is
		     * NOT monotonic increasing ... this matches what pmval(1)
		     * and pmie(1) do in the same circumstances.
		     */
		    struct timespec	stampdiff;

		    stampdiff = np->data.info->stamp;
		    pmtimespecDec(&stampdiff, &np->data.info->last_stamp);
		    switch (np->left->desc.type) {
			case PM_TYPE_32:
			    np->data.info->ivlist[k].value.d = (double)(np->left->data.info->ivlist[i].value.l - np->left->data.info->last_ivlist[j].value.l);
			    break;
			case PM_TYPE_U32:
			    if (np->left->desc.sem == PM_SEM_COUNTER &&
				np->left->data.info->ivlist[i].value.ul < np->left->data.info->last_ivlist[j].value.ul)
				continue;
			    np->data.info->ivlist[k].value.d = (double)(np->left->data.info->ivlist[i].value.ul - np->left->data.info->last_ivlist[j].value.ul);
			    break;
			case PM_TYPE_64:
			    if (np->left->desc.sem == PM_SEM_COUNTER &&
				np->left->data.info->ivlist[i].value.ll < np->left->data.info->last_ivlist[j].value.ll)
				continue;
			    np->data.info->ivlist[k].value.d = (double)(np->left->data.info->ivlist[i].value.ll - np->left->data.info->last_ivlist[j].value.ll);
			    break;
			case PM_TYPE_U64:
			    if (np->left->desc.sem == PM_SEM_COUNTER &&
				np->left->data.info->ivlist[i].value.ull < np->left->data.info->last_ivlist[j].value.ull)
				continue;
			    np->data.info->ivlist[k].value.d = (double)(np->left->data.info->ivlist[i].value.ull - np->left->data.info->last_ivlist[j].value.ull);
			    break;
			case PM_TYPE_FLOAT:
			    if (np->left->desc.sem == PM_SEM_COUNTER &&
				np->left->data.info->ivlist[i].value.f < np->left->data.info->last_ivlist[j].value.f)
				continue;
			    np->data.info->ivlist[k].value.d = (double)(np->left->data.info->ivlist[i].value.f - np->left->data.info->last_ivlist[j].value.f);
			    break;
			case PM_TYPE_DOUBLE:
			    if (np->left->desc.sem == PM_SEM_COUNTER &&
				np->left->data.info->ivlist[i].value.d < np->left->data.info->last_ivlist[j].value.d)
				continue;
			    np->data.info->ivlist[k].value.d = np->left->data.info->ivlist[i].value.d - np->left->data.info->last_ivlist[j].value.d;
			    break;
			default:
			    /*
			     * Nothing should end up here as check_expr() checks
			     * for numeric data type at bind time
			     */
			    return PM_ERR_CONV;
		    }
		    np->data.info->ivlist[k].value.d /= pmtimespecToReal(&stampdiff);
		    /*
		     * check_expr() ensures dimTime is 0 or 1 at bind time
		     */
		    if (np->left->desc.units.dimTime == 1) {
			/* scale rate(time counter) -> time utilization */
			if (np->data.info->time_scale < 0) {
			    /*
			     * one trip initialization for time utilization
			     * scaling factor (to scale metric from counter
			     * units into seconds)
			     */
			    int		n;
			    np->data.info->time_scale = 1;
			    if (np->left->desc.units.scaleTime > PM_TIME_SEC) {
				for (n = PM_TIME_SEC; n < np->left->desc.units.scaleTime; n++)
				    np->data.info->time_scale *= 60;
			    }
			    else {
				for (n = np->left->desc.units.scaleTime; n < PM_TIME_SEC; n++)
				    np->data.info->time_scale /= 1000;
			    }
			}
			np->data.info->ivlist[k].value.d *= np->data.info->time_scale;
		    }
		}
		k++;
	    }
	    np->data.info->numval = k;
	    return np->data.info->numval;

	case N_NOT:	/* boolean negation, values are in the left expr */
	    assert(np->left != NULL);
	    free_ivlist(np);
	    np->data.info->numval = np->left->data.info->numval;
	    if (np->data.info->numval <= 0)
		return np->data.info->numval;
	    if ((np->data.info->ivlist = (val_t *)malloc(np->data.info->numval*sizeof(val_t))) == NULL) {
		pmNoMem("eval_expr: N_NOT ivlist", np->data.info->numval*sizeof(val_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    /*
	     * ivlist[i] = ! left->ivlist[i]
	     */
	    for (i = 0; i < np->data.info->numval; i++) {
		switch (np->left->desc.type) {
		    case PM_TYPE_32:
			np->data.info->ivlist[i].value.ul = (np->left->data.info->ivlist[i].value.l == 0);
			break;
		    case PM_TYPE_U32:
			np->data.info->ivlist[i].value.ul = (np->left->data.info->ivlist[i].value.ul == 0);
			break;
		    case PM_TYPE_64:
			np->data.info->ivlist[i].value.ul = (np->left->data.info->ivlist[i].value.ll == 0);
			break;
		    case PM_TYPE_U64:
			np->data.info->ivlist[i].value.ul = (np->left->data.info->ivlist[i].value.ull == 0);
			break;
		    case PM_TYPE_FLOAT:
			np->data.info->ivlist[i].value.ul = (np->left->data.info->ivlist[i].value.f == 0);
			break;
		    case PM_TYPE_DOUBLE:
			np->data.info->ivlist[i].value.ul = (np->left->data.info->ivlist[i].value.d == 0);
			break;
		}
		np->data.info->ivlist[i].inst = np->left->data.info->ivlist[i].inst;
	    }
	    return np->data.info->numval;

	case N_NEG:	/* unary arithmetic negation */
	    assert(np->left != NULL);
	    free_ivlist(np);
	    np->data.info->numval = np->left->data.info->numval;
	    if (np->data.info->numval <= 0)
		return np->data.info->numval;
	    if ((np->data.info->ivlist = (val_t *)malloc(np->data.info->numval*sizeof(val_t))) == NULL) {
		pmNoMem("eval_expr: N_NEG ivlist", np->data.info->numval*sizeof(val_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    /*
	     * ivlist[i] = - left->ivlist[i]
	     */
	    for (i = 0; i < np->data.info->numval; i++) {
		switch (np->left->desc.type) {
		    case PM_TYPE_32:
			np->data.info->ivlist[i].value.l = -np->left->data.info->ivlist[i].value.l;
			break;
		    case PM_TYPE_U32:
			np->data.info->ivlist[i].value.l = -np->left->data.info->ivlist[i].value.ul;
			break;
		    case PM_TYPE_64:
			np->data.info->ivlist[i].value.ll = -np->left->data.info->ivlist[i].value.ll;
			break;
		    case PM_TYPE_U64:
			np->data.info->ivlist[i].value.ll = -np->left->data.info->ivlist[i].value.ull;
			break;
		    case PM_TYPE_FLOAT:
			np->data.info->ivlist[i].value.f = -np->left->data.info->ivlist[i].value.f;
			break;
		    case PM_TYPE_DOUBLE:
			np->data.info->ivlist[i].value.d = -np->left->data.info->ivlist[i].value.d;
			break;
		}
		np->data.info->ivlist[i].inst = np->left->data.info->ivlist[i].inst;
	    }
	    return np->data.info->numval;

	case N_COLON:
	    /* do nothing */
	    return 0;

	case N_QUEST:
	    assert(np->left != NULL);
	    assert(np->right != NULL);
	    assert(np->right->left != NULL);
	    assert(np->right->right != NULL);
	    free_ivlist(np);
	    {
		node_t	*pick;
		node_t	*pick_inst;
		int	numval;
		/*
		 * the ternary expression is only going to have well-behaved
		 * semantics if we have value(s) for the guard and both the
		 * truth/false expressions (unless the these are a novalue()
		 * node or QUEST_BIND_LEFT or QUEST_BIND_RIGHT are in play)
		 */
		if (pmDebugOptions.derive && pmDebugOptions.appl2 && pmDebugOptions.desperate) {
		    fprintf(stderr, "eval_expr: ? bind %d values: guard %d left %s %d",
			np->right->data.info->bind, np->left->data.info->numval,
			__dmnode_type_str(np->right->left->type),
			np->right->left->data.info->numval);
		    fprintf(stderr, " right %s %d\n",
			__dmnode_type_str(np->right->right->type),
			np->right->right->data.info->numval);
		}
		if (np->left->data.info->numval <= 0) {
		    /* no guard expression values */
		    np->data.info->numval = np->left->data.info->numval;
		    return np->data.info->numval;
		}
		if (np->right->left->data.info->numval <= 0 && np->right->left->type != N_NOVALUE && (np->right->data.info->bind & QUEST_BIND_LEFT)) {
		    /* no true expression values */
		    np->data.info->numval = np->right->left->data.info->numval;
		    return np->data.info->numval;
		}
		if (np->right->right->data.info->numval <= 0 && np->right->right->type != N_NOVALUE && (np->right->data.info->bind & QUEST_BIND_RIGHT)) {
		    /* no false expression values */
		    np->data.info->numval = np->right->right->data.info->numval;
		    return np->data.info->numval;
		}
		if (np->right->data.info->bind == QUEST_BIND_LEFT) {
		    /* only looking at <left-expr> */
		    numval = np->right->left->data.info->numval;
		    pick = np->right->left;
		    pick_inst = np->right->left;
		}
		else if (np->right->data.info->bind == QUEST_BIND_RIGHT) {
		    /* only looking at <right-expr> */
		    numval = np->right->right->data.info->numval;
		    pick = np->right->right;
		    pick_inst = np->right->right;
		}
		else {
		    /* maybe looking at <left-expr> and <right-expr> */
		    numval = np->right->left->data.info->numval;
		    if (np->right->right->data.info->numval > numval)
			numval = np->right->right->data.info->numval;
		    pick = NULL;
		    /* default indom choice, use true operand */
		    pick_inst = np->right->left;
		    if (np->right->left->desc.indom == PM_INDOM_NULL && np->right->right->desc.indom != PM_INDOM_NULL)
			/* use false operand */
			pick_inst = np->right->right;
		}
		np->data.info->numval = numval;
		if ((np->data.info->ivlist = (val_t *)malloc(numval*sizeof(val_t))) == NULL) {
		    pmNoMem("eval_expr: N_QUEST ivlist", numval*sizeof(val_t), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		/*
		 * if guard, true and false operands are a mix of singular
		 * values and values with an indom, need to use one of the
		 * indom ones to provide instances for the result ... note
		 * we've previously established (at bind time) that at most
		 * one indom is mentioned across all three operands.
		 *
		 * guard is left operand and value is arithmetic
		 */
		for (i = 0; i < numval; i++) {
		    if (np->right->data.info->bind == QUEST_BIND_BOTH) {
			/*
			 * first-time for singular guard, else if guard
			 * has indom evaluate the guard for each instance
			 * and set pick (<left-epxr> or <right-expr>)
			 */
			if (i < np->left->data.info->numval) {
			    switch (np->left->desc.type) {
				case PM_TYPE_32:
				    if (np->left->data.info->ivlist[i].value.l != 0)
					pick = np->right->left;
				    else
					pick = np->right->right;
				    break;
				case PM_TYPE_U32:
				    if (np->left->data.info->ivlist[i].value.ul != 0)
					pick = np->right->left;
				    else
					pick = np->right->right;
				    break;
				case PM_TYPE_64:
				    if (np->left->data.info->ivlist[i].value.ll != 0)
					pick = np->right->left;
				    else
					pick = np->right->right;
				    break;
				case PM_TYPE_U64:
				    if (np->left->data.info->ivlist[i].value.ull != 0)
					pick = np->right->left;
				    else
					pick = np->right->right;
				    break;
				case PM_TYPE_FLOAT:
				    if (np->left->data.info->ivlist[i].value.f != 0)
					pick = np->right->left;
				    else
					pick = np->right->right;
				    break;
				case PM_TYPE_DOUBLE:
				    if (np->left->data.info->ivlist[i].value.d != 0)
					pick = np->right->left;
				    else
					pick = np->right->right;
				    break;
				default:
				    if (pmDebugOptions.derive) {
					fprintf(stderr, "eval_expr: botch: drived metric %s: guard has odd type (%d)\n", pmIDStr_r(np->data.info->pmid, strbuf, sizeof(strbuf)), np->left->desc.type);
				    }
				    return PM_ERR_TYPE;
			    }
			}
		    }
		    /* fallthrough for singular guard and use same pick */
		    if (pick == NULL) {
			fprintf(stderr, "eval_expr: botch: picked nothing\n"); 
			__dmdumpexpr(np, 0);
		    }
		    assert(pick != NULL);
		    if (pick->type == N_NOVALUE) {
			np->data.info->numval--;
			continue;
		    }
		    if (pmDebugOptions.derive && pmDebugOptions.appl2 && pmDebugOptions.desperate) {
			fprintf(stderr, "pick inst[%d] %s numval=%d\n",
			    i, __dmnode_type_str(pick->type), pick->data.info->numval);
		    }
		    switch (np->desc.type) {
			case PM_TYPE_32:
			    if (i < pick->data.info->numval)
				np->data.info->ivlist[i].value.l = pick->data.info->ivlist[i].value.l;
			    else
				np->data.info->ivlist[i].value.l = pick->data.info->ivlist[0].value.l;
			    break;
			case PM_TYPE_U32:
			    if (i < pick->data.info->numval)
				np->data.info->ivlist[i].value.ul = pick->data.info->ivlist[i].value.ul;
			    else
				np->data.info->ivlist[i].value.ul = pick->data.info->ivlist[0].value.ul;
			    break;
			case PM_TYPE_64:
			    if (i < pick->data.info->numval)
				np->data.info->ivlist[i].value.ll = pick->data.info->ivlist[i].value.ll;
			    else
				np->data.info->ivlist[i].value.ll = pick->data.info->ivlist[0].value.ll;
			    break;
			case PM_TYPE_U64:
			    if (i < pick->data.info->numval)
				np->data.info->ivlist[i].value.ull = pick->data.info->ivlist[i].value.ull;
			    else
				np->data.info->ivlist[i].value.ull = pick->data.info->ivlist[0].value.ull;
			    break;
			case PM_TYPE_FLOAT:
			    if (i < pick->data.info->numval)
				np->data.info->ivlist[i].value.f = pick->data.info->ivlist[i].value.f;
			    else
				np->data.info->ivlist[i].value.f = pick->data.info->ivlist[0].value.f;
			    break;
			case PM_TYPE_DOUBLE:
			    if (i < pick->data.info->numval)
				np->data.info->ivlist[i].value.d = pick->data.info->ivlist[i].value.d;
			    else
				np->data.info->ivlist[i].value.d = pick->data.info->ivlist[0].value.d;
			    break;
			case PM_TYPE_STRING:
			    if (i < pick->data.info->numval)
				np->data.info->ivlist[i].value.cp = pick->data.info->ivlist[i].value.cp;
			    else
				np->data.info->ivlist[i].value.cp = pick->data.info->ivlist[0].value.cp;
			    break;
			default:
			    if (pmDebugOptions.derive) {
				fprintf(stderr, "eval_expr: botch: drived metric %s: value has odd type (%d)\n", pmIDStr_r(np->data.info->pmid, strbuf, sizeof(strbuf)), np->left->desc.type);
			    }
			    return PM_ERR_TYPE;
		    }
		    np->data.info->ivlist[i].inst = pick_inst->data.info->ivlist[i].inst;
		}
	    }
	    if (np->data.info->numval == 0) {
		free(np->data.info->ivlist);
		np->data.info->ivlist = NULL;
	    }
	    return np->data.info->numval;

	case N_RESCALE:
	    assert(np->left != NULL);
	    free_ivlist(np);
	    np->data.info->numval = np->left->data.info->numval;
	    if (np->data.info->numval <= 0)
		return np->data.info->numval;
	    if ((np->data.info->ivlist = (val_t *)malloc(np->data.info->numval*sizeof(val_t))) == NULL) {
		pmNoMem("eval_expr: N_RESCALE ivlist", np->data.info->numval*sizeof(val_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    /*
	     * ivlist[i] = rescale(left->ivlist[i], right->desc.units)
	     */
	    for (j = 0, i = 0; i < np->data.info->numval; i++) {
		sts = pmConvScale(np->desc.type,
		    &np->left->data.info->ivlist[i].value, &np->left->desc.units,
		    &np->data.info->ivlist[j].value, &np->right->desc.units);
		np->data.info->ivlist[j].inst = np->left->data.info->ivlist[i].inst;
		if (sts >= 0)
		    j++;
	    }
	    np->data.info->numval = j;
	    return np->data.info->numval;

	case N_INSTANT:
	    /*
	     * values are in the left expr
	     */
	    assert(np->left != NULL);
	    save_ivlist(np);
	    np->data.info->last_stamp = np->data.info->stamp;
	    np->data.info->stamp = *stamp;
	    np->data.info->numval = np->left->data.info->numval;
	    if (np->data.info->numval > 0)
		np->data.info->ivlist = np->left->data.info->ivlist;
	    return np->data.info->numval;

	case N_AVG:
	case N_COUNT:
	case N_SUM:
	case N_MAX:
	case N_MIN:
	case N_SCALAR:
	    save_ivlist(np);
	    if (np->data.info->ivlist == NULL) {
		/* initialize ivlist[] for singular instance first time through */
		if ((np->data.info->ivlist = (val_t *)malloc(sizeof(val_t))) == NULL) {
		    pmNoMem("eval_expr: aggr ivlist", sizeof(val_t), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		np->data.info->ivlist[0].inst = PM_IN_NULL;
	    }
	    /*
	     * values are in the left expr
	     */
	    if (np->type == N_COUNT) {
		np->data.info->numval = 1;
		np->data.info->ivlist[0].value.l = np->left->data.info->numval;
	    }
	    else if (np->type == N_SCALAR) {
		np->data.info->numval = np->left->data.info->numval;
		if (np->data.info->numval <= 0) {
		    /* nothing to be done ... */
		    ;
		}
		else {
		    if (np->data.info->numval > 1)
			/* pick first instance only */
			np->data.info->numval = 1;
		    np->data.info->ivlist[0].inst = PM_IN_NULL;
		    np->data.info->ivlist[0].value = np->left->data.info->ivlist[0].value;
		    np->data.info->ivlist[0].vlen = np->left->data.info->ivlist[0].vlen;
		}
	    }
	    else {
		np->data.info->numval = 1;
		if (np->type == N_AVG)
		    np->data.info->ivlist[0].value.f = 0;
		else if (np->type == N_SUM) {
		    switch (np->desc.type) {
			case PM_TYPE_32:
			    np->data.info->ivlist[0].value.l = 0;
			    break;
			case PM_TYPE_U32:
			    np->data.info->ivlist[0].value.ul = 0;
			    break;
			case PM_TYPE_64:
			    np->data.info->ivlist[0].value.ll = 0;
			    break;
			case PM_TYPE_U64:
			    np->data.info->ivlist[0].value.ull = 0;
			    break;
			case PM_TYPE_FLOAT:
			    np->data.info->ivlist[0].value.f = 0;
			    break;
			case PM_TYPE_DOUBLE:
			    np->data.info->ivlist[0].value.d = 0;
			    break;
		    }
		}
		for (i = 0; i < np->left->data.info->numval; i++) {
		    switch (np->type) {

			case N_AVG:
			    switch (np->left->desc.type) {
				case PM_TYPE_32:
				    np->data.info->ivlist[0].value.f += (float)np->left->data.info->ivlist[i].value.l / np->left->data.info->numval;
				    break;
				case PM_TYPE_U32:
				    np->data.info->ivlist[0].value.f += (float)np->left->data.info->ivlist[i].value.ul / np->left->data.info->numval;
				    break;
				case PM_TYPE_64:
				    np->data.info->ivlist[0].value.f += (float)np->left->data.info->ivlist[i].value.ll / np->left->data.info->numval;
				    break;
				case PM_TYPE_U64:
				    np->data.info->ivlist[0].value.f += (float)np->left->data.info->ivlist[i].value.ull / np->left->data.info->numval;
				    break;
				case PM_TYPE_FLOAT:
				    np->data.info->ivlist[0].value.f += (float)np->left->data.info->ivlist[i].value.f / np->left->data.info->numval;
				    break;
				case PM_TYPE_DOUBLE:
				    np->data.info->ivlist[0].value.f += (float)np->left->data.info->ivlist[i].value.d / np->left->data.info->numval;
				    break;
				default:
				    /*
				     * check_expr() checks for numeric data
				     * type at bind time ... if here, botch!
				     */
				    return PM_ERR_CONV;
			    }
			    break;

			case N_MAX:
			    switch (np->desc.type) {
				case PM_TYPE_32:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.l < np->left->data.info->ivlist[i].value.l)
					np->data.info->ivlist[0].value.l = np->left->data.info->ivlist[i].value.l;
				    break;
				case PM_TYPE_U32:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.ul < np->left->data.info->ivlist[i].value.ul)
					np->data.info->ivlist[0].value.ul = np->left->data.info->ivlist[i].value.ul;
				    break;
				case PM_TYPE_64:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.ll < np->left->data.info->ivlist[i].value.ll)
					np->data.info->ivlist[0].value.ll = np->left->data.info->ivlist[i].value.ll;
				    break;
				case PM_TYPE_U64:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.ull < np->left->data.info->ivlist[i].value.ull)
					np->data.info->ivlist[0].value.ull = np->left->data.info->ivlist[i].value.ull;
				    break;
				case PM_TYPE_FLOAT:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.f < np->left->data.info->ivlist[i].value.f)
					np->data.info->ivlist[0].value.f = np->left->data.info->ivlist[i].value.f;
				    break;
				case PM_TYPE_DOUBLE:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.d < np->left->data.info->ivlist[i].value.d)
					np->data.info->ivlist[0].value.d = np->left->data.info->ivlist[i].value.d;
				    break;
				default:
				    /*
				     * check_expr() checks for numeric data
				     * type at bind time ... if here, botch!
				     */
				    return PM_ERR_CONV;
			    }
			    break;

			case N_MIN:
			    switch (np->desc.type) {
				case PM_TYPE_32:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.l > np->left->data.info->ivlist[i].value.l)
					np->data.info->ivlist[0].value.l = np->left->data.info->ivlist[i].value.l;
				    break;
				case PM_TYPE_U32:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.ul > np->left->data.info->ivlist[i].value.ul)
					np->data.info->ivlist[0].value.ul = np->left->data.info->ivlist[i].value.ul;
				    break;
				case PM_TYPE_64:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.ll > np->left->data.info->ivlist[i].value.ll)
					np->data.info->ivlist[0].value.ll = np->left->data.info->ivlist[i].value.ll;
				    break;
				case PM_TYPE_U64:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.ull > np->left->data.info->ivlist[i].value.ull)
					np->data.info->ivlist[0].value.ull = np->left->data.info->ivlist[i].value.ull;
				    break;
				case PM_TYPE_FLOAT:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.f > np->left->data.info->ivlist[i].value.f)
					np->data.info->ivlist[0].value.f = np->left->data.info->ivlist[i].value.f;
				    break;
				case PM_TYPE_DOUBLE:
				    if (i == 0 ||
				        np->data.info->ivlist[0].value.d > np->left->data.info->ivlist[i].value.d)
					np->data.info->ivlist[0].value.d = np->left->data.info->ivlist[i].value.d;
				    break;
				default:
				    /*
				     * check_expr() checks for numeric data
				     * type at bind time ... if here, botch!
				     */
				    return PM_ERR_CONV;
			    }
			    break;

			case N_SUM:
			    switch (np->desc.type) {
				case PM_TYPE_32:
				    np->data.info->ivlist[0].value.l += np->left->data.info->ivlist[i].value.l;
				    break;
				case PM_TYPE_U32:
				    np->data.info->ivlist[0].value.ul += np->left->data.info->ivlist[i].value.ul;
				    break;
				case PM_TYPE_64:
				    np->data.info->ivlist[0].value.ll += np->left->data.info->ivlist[i].value.ll;
				    break;
				case PM_TYPE_U64:
				    np->data.info->ivlist[0].value.ull += np->left->data.info->ivlist[i].value.ull;
				    break;
				case PM_TYPE_FLOAT:
				    np->data.info->ivlist[0].value.f += np->left->data.info->ivlist[i].value.f;
				    break;
				case PM_TYPE_DOUBLE:
				    np->data.info->ivlist[0].value.d += np->left->data.info->ivlist[i].value.d;
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
	    return np->data.info->numval;

	case N_NAME:
	    /* fastpath for pmid == PM_ID_NULL case (from QUEST_BIND_LAZY) */
	    if (np->data.info->pmid == PM_ID_NULL) {
		return 0;
	    }
	    free_ivlist(np);
	    /*
	     * otherwise extract instance-values from pmResult and store
	     * them in ivlist[] as <int, pmAtomValue> pairs
	     */
	    for (j = 0; j < numpmid; j++) {
		if (np->data.info->pmid == vset[j]->pmid) {
		    np->data.info->numval = vset[j]->numval;
		    if (np->data.info->numval <= 0)
			return np->data.info->numval;
		    if ((np->data.info->ivlist = (val_t *)malloc(np->data.info->numval*sizeof(val_t))) == NULL) {
			pmNoMem("eval_expr: metric ivlist", np->data.info->numval*sizeof(val_t), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    for (i = 0; i < np->data.info->numval; i++) {
			np->data.info->ivlist[i].inst = vset[j]->vlist[i].inst;
			switch (np->desc.type) {
			    case PM_TYPE_32:
			    case PM_TYPE_U32:
				np->data.info->ivlist[i].value.l = vset[j]->vlist[i].value.lval;
				break;
			    case PM_TYPE_64:
			    case PM_TYPE_U64:
				if (vset[j]->valfmt != PM_VAL_DPTR && vset[j]->valfmt != PM_VAL_SPTR)
				    return PM_ERR_LOGREC;
				memcpy((void *)&np->data.info->ivlist[i].value.ll, (void *)vset[j]->vlist[i].value.pval->vbuf, sizeof(__int64_t));
				break;
			    case PM_TYPE_FLOAT:
				if (vset[j]->valfmt == PM_VAL_INSITU) {
				    /* old style insitu float */
				    np->data.info->ivlist[i].value.l = vset[j]->vlist[i].value.lval;
				}
				else if (vset[j]->valfmt == PM_VAL_DPTR || vset[j]->valfmt == PM_VAL_SPTR) {
				    assert(vset[j]->vlist[i].value.pval->vtype == PM_TYPE_FLOAT);
				    memcpy((void *)&np->data.info->ivlist[i].value.f, (void *)vset[j]->vlist[i].value.pval->vbuf, sizeof(float));
				}
				else
				    return PM_ERR_LOGREC;
				break;
			    case PM_TYPE_DOUBLE:
				if (vset[j]->valfmt != PM_VAL_DPTR && vset[j]->valfmt != PM_VAL_SPTR)
				    return PM_ERR_LOGREC;
				memcpy((void *)&np->data.info->ivlist[i].value.d, (void *)vset[j]->vlist[i].value.pval->vbuf, sizeof(double));
				break;
			    case PM_TYPE_STRING:
				if (vset[j]->valfmt != PM_VAL_DPTR && vset[j]->valfmt != PM_VAL_SPTR)
				    return PM_ERR_LOGREC;
				need = vset[j]->vlist[i].value.pval->vlen-PM_VAL_HDR_SIZE;
				if ((np->data.info->ivlist[i].value.cp = (char *)malloc(need)) == NULL) {
				    pmNoMem("eval_expr: string value", vset[j]->vlist[i].value.pval->vlen, PM_FATAL_ERR);
				    /*NOTREACHED*/
				}
				memcpy((void *)np->data.info->ivlist[i].value.cp, (void *)vset[j]->vlist[i].value.pval->vbuf, need);
				np->data.info->ivlist[i].vlen = need;
				break;
			    case PM_TYPE_AGGREGATE:
			    case PM_TYPE_AGGREGATE_STATIC:
			    case PM_TYPE_EVENT:
			    case PM_TYPE_HIGHRES_EVENT:
				if (vset[j]->valfmt != PM_VAL_DPTR && vset[j]->valfmt != PM_VAL_SPTR)
				    return PM_ERR_LOGREC;
				if ((np->data.info->ivlist[i].value.vbp = (pmValueBlock *)malloc(vset[j]->vlist[i].value.pval->vlen)) == NULL) {
				    pmNoMem("eval_expr: aggregate value", vset[j]->vlist[i].value.pval->vlen, PM_FATAL_ERR);
				    /*NOTREACHED*/
				}
				memcpy(np->data.info->ivlist[i].value.vbp, (void *)vset[j]->vlist[i].value.pval, vset[j]->vlist[i].value.pval->vlen);
				np->data.info->ivlist[i].vlen = vset[j]->vlist[i].value.pval->vlen;
				break;
			    default:
				/*
				 * really only PM_TYPE_NOSUPPORT should
				 * end up here
				 */
				return PM_ERR_TYPE;
			}
		    }
		    return np->data.info->numval;
		}
	    }
	    if (pmDebugOptions.derive)
		fprintf(stderr, "eval_expr: botch: operand %s not in the extended pmResult\n", pmIDStr_r(np->data.info->pmid, strbuf, sizeof(strbuf)));
	    return PM_ERR_PMID;

	case N_DEFINED:
	    /* already setup from check_expr(), nothing to do ... */
	    return np->data.info->numval;

	case N_ANON:
	    /* no values available for anonymous metrics */
	    return 0;

	case N_SCALE:
	    /* no associated values */
	    return 0;

	case N_FILTERINST:
	    /*
	     * possible values are in the right expr
	     */
	    assert(np->left != NULL);
	    assert(np->right != NULL);
	    np->data.info->last_stamp = np->data.info->stamp;
	    np->data.info->stamp = *stamp;
	    if (np->left->data.pattern->ftype == F_REGEX)
		free_ivlist(np);
	    else
		np->data.info->numval = 0;
	    for (i = 0; i < np->right->data.info->numval; i++) {
		if (np->left->data.pattern->ftype == F_REGEX) {
		    /* regular expression match */
		    __pmHashNode	*hp;
		    instctl_t		*ip;
		    char		*iname;
		    char		*q;

		    if ((hp = __pmHashSearch(np->right->data.info->ivlist[i].inst, &np->left->data.pattern->hash)) == NULL) {
			__pmTimestamp	save_origin;

			/* first time we've seen this inst for this expr node */
			if ((ip = (instctl_t *)malloc(sizeof(instctl_t))) == NULL) {
			    pmNoMem("eval_expr: inst_ctl", sizeof(instctl_t), PM_FATAL_ERR);
			    /*NOTREACHED*/
			}
			ip->inst = np->right->data.info->ivlist[i].inst;
			ip->used = 0;
			if (ctxp->c_type == PM_CONTEXT_ARCHIVE) {
			    /*
			     * Need to update context timestamp origin so that
			     * indom search will succeed ... and then put it
			     * back.
			     */
			    save_origin = ctxp->c_origin;	/* struct assignment */
			    ctxp->c_origin.sec = stamp->tv_sec;
			    ctxp->c_origin.nsec = stamp->tv_nsec;
			}
			sts = pmNameInDom_ctx(ctxp, np->right->desc.indom, ip->inst, &iname);
			if (ctxp->c_type == PM_CONTEXT_ARCHIVE)
			    ctxp->c_origin = save_origin;	/* struct assignment */
			if (sts >= 0) {
			    /*
			     * classical external instance name matching means
			     * strip any text after the first space
			     */
			    for (q = iname; *q && *q != ' '; q++)
				;
			    *q = '\0';
			    if (regexec(&np->left->data.pattern->regex, iname, 0, NULL, 0) == 0)
				ip->match = np->left->data.pattern->invert ? 0 : 1;
			    else
				ip->match = np->left->data.pattern->invert ? 1 : 0;
			    free(iname);
			}
			else {
			    /* pmNameInDom() failed ... this should not happen */
			    if (pmDebugOptions.derive && pmDebugOptions.appl2) {
				char	errmsg[PM_MAXERRMSGLEN];
				fprintf(stderr, "eval_expr: expr node " PRINTF_P_PFX "%p type=%s", np, __dmnode_type_str(np->type));
				fprintf(stderr, " pmNameInDom(%s, %d, ...) failed:",
				    pmInDomStr_r(np->right->desc.indom, strbuf, sizeof(strbuf)),
				    ip->inst);
				fprintf(stderr, " %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
			    }
			    ip->match = 0;
			}
			if ((sts = __pmHashAdd(np->right->data.info->ivlist[i].inst, (void *)ip, &np->left->data.pattern->hash)) < 0) {
			    /* also, should not happen */
			    if (pmDebugOptions.derive && pmDebugOptions.appl2) {
				char	errmsg[PM_MAXERRMSGLEN];
				fprintf(stderr, "eval_expr: expr node " PRINTF_P_PFX "%p type=%s", np, __dmnode_type_str(np->type));
				fprintf(stderr, " __pmHashAdd(%d, ...) failed:",
				    np->right->data.info->ivlist[i].inst);
				fprintf(stderr, " %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
			    }
			    ip->match = 0;
			}
		    }
		    else
			ip = (instctl_t *)hp->data;
		    ip->used++;
		    if (ip->match) {
			val_t	*tmp_ivlist;
			np->data.info->numval++;
			if ((tmp_ivlist = (val_t *)realloc(np->data.info->ivlist, np->data.info->numval*sizeof(val_t))) == NULL) {
			    pmNoMem("eval_expr: PATTERN ivlist", np->data.info->numval*sizeof(val_t), PM_FATAL_ERR);
			    /*NOTREACHED*/
			}
			np->data.info->ivlist = tmp_ivlist;
			np->data.info->ivlist[np->data.info->numval-1] = np->right->data.info->ivlist[i];
		    }
		}
		else {
		    /* F_EXACT ... simple text match */
		    if (np->left->data.pattern->inst == PM_IN_NULL) {
			/* need to map external name to internal instance id */
			sts = pmLookupInDom_ctx(ctxp, np->right->desc.indom, np->left->value);
			if (sts < 0) {
			    /*
			     * instance is not in the indom at this point
			     * in time, so no point walking the right
			     * operand's instances, just return numvla == 0
			     */
			    if (pmDebugOptions.derive && pmDebugOptions.appl2) {
				char	errmsg[PM_MAXERRMSGLEN];
				fprintf(stderr, "eval_expr: expr node " PRINTF_P_PFX "%p type=%s", np, __dmnode_type_str(np->type));
				fprintf(stderr, " pmLookupInDom_ctx(...,%s,%s) failed:",
				    pmInDomStr_r(np->right->desc.indom, errmsg, sizeof(errmsg)), np->left->value);
				fprintf(stderr, " %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
			    }
			    break;
			}
			else
			    np->left->data.pattern->inst = sts;
		    }
		    if (np->data.info->ivlist == NULL) {
			/* first time, at most one value here */
			if ((np->data.info->ivlist = (val_t *)malloc(sizeof(val_t))) == NULL) {
			    pmNoMem("eval_expr: filterinst ivlist", sizeof(val_t), PM_FATAL_ERR);
			    /*NOTREACHED*/
			}
		    }
		    if (np->left->data.pattern->inst == np->right->data.info->ivlist[i].inst) {
			np->data.info->ivlist[0] = np->right->data.info->ivlist[i];
			np->data.info->numval = 1;
			break;
		    }
		}
	    }
	    if (np->left->data.pattern->ftype == F_REGEX) {
		np->left->data.pattern->used++;
		if (np->left->data.pattern->used >= REGEX_INST_COMPACT)
		    regex_inst_gc(np->left->data.pattern);
	    }
	    return np->data.info->numval;

	case N_PATTERN:
	    /* do nothing */
	    return 0;

	case N_NOVALUE:
	    /* do nothing */
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
	    if (np->left->data.info->numval <= 0) {
		np->data.info->numval = 0;
		return np->data.info->numval;
	    }
	    if (np->right->data.info->numval <= 0) {
		np->data.info->numval = 0;
		return np->data.info->numval;
	    }
	    /*
	     * really got some work to do ...
	     */
	    if (np->left->desc.indom == PM_INDOM_NULL)
		np->data.info->numval = np->right->data.info->numval;
	    else if (np->right->desc.indom == PM_INDOM_NULL)
		np->data.info->numval = np->left->data.info->numval;
	    else {
		/*
		 * Generally have the same number of instances because
		 * both operands are over the same instance domain,
		 * fetched with the same profile.  When not the case,
		 * the result can contain no more instances than in
		 * the smaller of the operands.
		 */
		if (np->left->data.info->numval <= np->right->data.info->numval)
		    np->data.info->numval = np->left->data.info->numval;
		else
		    np->data.info->numval = np->right->data.info->numval;
	    }
	    if ((np->data.info->ivlist = (val_t *)malloc(np->data.info->numval*sizeof(val_t))) == NULL) {
		pmNoMem("eval_expr: expr ivlist", np->data.info->numval*sizeof(val_t), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    /*
	     * ivlist[k] = left->ivlist[i] <op> right->ivlist[j]
	     */
	    for (i = j = k = 0; k < np->data.info->numval; ) {
		if (i >= np->left->data.info->numval || j >= np->right->data.info->numval) {
		    /* run out of operand instances, quit */
		    np->data.info->numval = k;
		    break;
		}
		if (np->left->desc.indom != PM_INDOM_NULL &&
		    np->right->desc.indom != PM_INDOM_NULL) {
		    if (np->left->data.info->ivlist[i].inst != np->right->data.info->ivlist[j].inst) {
			/*
			 * left ith inst != right jth inst ... search in right
			 * (this is sort of expected for FILTERINST nodes)
			 */
			if ((pmDebugOptions.derive && pmDebugOptions.appl2) &&
			    np->left->type != N_FILTERINST &&
			    np->right->type != N_FILTERINST) {
			    fprintf(stderr, "eval_expr: %s: inst[%d] mismatch left [%d]=%d right [%d]=%d\n",
				__dmnode_type_str(np->type), k,
				i, np->left->data.info->ivlist[i].inst,
				j, np->right->data.info->ivlist[j].inst);
			}
			for (j = 0; j < np->right->data.info->numval; j++) {
			    if (np->left->data.info->ivlist[i].inst == np->right->data.info->ivlist[j].inst)
				break;
			}
			if (j == np->right->data.info->numval) {
			    /*
			     * no match, so next instance on left operand,
			     * and reset to start from first instance of
			     * right operand
			     */
			    i++;
			    j = 0;
			    continue;
			}
			else {
			    if ((pmDebugOptions.derive && pmDebugOptions.appl2) &&
				np->left->type != N_FILTERINST &&
				np->right->type != N_FILTERINST) {
				fprintf(stderr, "eval_expr: recover @ right [%d]=%d\n", j, np->right->data.info->ivlist[j].inst);
			    }
			}
		    }
		}
		if (np->type == N_LT || np->type == N_LEQ || np->type == N_EQ ||
		    np->type == N_GEQ || np->type == N_GT || np->type == N_NEQ ||
		    np->type == N_AND || np->type == N_OR) {
		    /*
		     * relational and boolean operators need to perform
		     * the comparions with operand type promotion, but
		     * then cast the result back to a U32 value
		     */
		    int	res_type = promote[np->left->desc.type][np->right->desc.type];
		    pmAtomValue	res;
		    res = bin_op(res_type, np->type,
			   np->left->data.info->ivlist[i].value, np->left->desc.type,
			   np->left->data.info->mul_scale, np->left->data.info->div_scale,
			   np->right->data.info->ivlist[j].value, np->right->desc.type,
			   np->right->data.info->mul_scale, np->right->data.info->div_scale);
		    switch (res_type) {
			case PM_TYPE_32:
			    np->data.info->ivlist[k].value.ul = (__uint32_t)res.l;
			    break;
			case PM_TYPE_U32:
			    np->data.info->ivlist[k].value.ul = (__uint32_t)res.ul;
			    break;
			case PM_TYPE_64:
			    np->data.info->ivlist[k].value.ul = (__uint32_t)res.ll;
			    break;
			case PM_TYPE_U64:
			    np->data.info->ivlist[k].value.ul = (__uint32_t)res.ull;
			    break;
			case PM_TYPE_FLOAT:
			    np->data.info->ivlist[k].value.ul = (__uint32_t)res.f;
			    break;
			case PM_TYPE_DOUBLE:
			    np->data.info->ivlist[k].value.ul = (__uint32_t)res.d;
			    break;
		    }
		}
		else {
		    /* arithmetic operators, just do it */
		    np->data.info->ivlist[k].value = bin_op(np->desc.type, np->type,
			   np->left->data.info->ivlist[i].value, np->left->desc.type,
			   np->left->data.info->mul_scale, np->left->data.info->div_scale,
			   np->right->data.info->ivlist[j].value, np->right->desc.type,
			   np->right->data.info->mul_scale, np->right->data.info->div_scale);
		}
		if (np->left->desc.indom != PM_INDOM_NULL)
		    np->data.info->ivlist[k].inst = np->left->data.info->ivlist[i].inst;
		else
		    np->data.info->ivlist[k].inst = np->right->data.info->ivlist[j].inst;
		k++;
		if (np->left->desc.indom != PM_INDOM_NULL) {
		    i++;
		    if (np->right->desc.indom != PM_INDOM_NULL) {
			j++;
			if (j >= np->right->data.info->numval) {
			    /* rescan if need be */
			    j = 0;
			}
		    }
		}
		else if (np->right->desc.indom != PM_INDOM_NULL) {
		    j++;
		}
	    }
	    return np->data.info->numval;

    }
    /*NOTREACHED*/
}

/*
 * Algorithm here is complicated by trying to re-write the pmValueSets
 * in a result structure (either pmResult or pmHighResResult).
 *
 * On entry the result is likely to be built over a pinned PDU buffer,
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
 * synthesize a pmResult there.
 */

static int
__dmpostvalueset(__pmContext *ctxp, struct timespec *stamp, int vnumpmid,
		pmValueSet **vset, int numpmid, pmValueSet **newvset)
{
    int		i, j, m;
    int		numval;
    int		valfmt;
    int		fails = 0;
    size_t	need;
    int		rewrite;
    ctl_t	*cp = (ctl_t *)ctxp->c_dm;

    for (j = 0; j < numpmid; j++) {
	numval = vset[j]->numval;
	valfmt = vset[j]->valfmt;
	rewrite = 0;
	/*
	 * pandering to gcc ... m is not used unless rewrite == 1 in
	 * which case m is well-defined
	 */
	m = 0;
	if (IS_DERIVED(vset[j]->pmid)) {
	    for (m = 0; m < cp->nmetric; m++) {
		if (vset[j]->pmid == cp->mlist[m].pmid) {
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

			numval = eval_expr(ctxp, cp->mlist[m].expr,
						stamp, vnumpmid, vset, 1);
			if (numval == PM_ERR_PMID)
			    fails++;

			if (pmDebugOptions.derive && pmDebugOptions.appl2) {
			    int		k, type = cp->mlist[m].expr->desc.type;
			    info_t	*info = cp->mlist[m].expr->data.info;
			    char	strbuf[20];
	
			    pmIDStr_r(vset[j]->pmid, strbuf, sizeof(strbuf));
			    fprintf(stderr, "%s: [%d] root node %s: numval=%d",
					    "__dmpostvalueset", j, strbuf, numval);
			    for (k = 0; k < numval; k++) {
				pmAtomValue value = info->ivlist[k].value;

				fprintf(stderr, " vset[%d]: inst=%d", k,
						info->ivlist[k].inst);
				if (type == PM_TYPE_32)
				    fprintf(stderr, " l=%d", value.l);
				else if (type == PM_TYPE_U32)
				    fprintf(stderr, " u=%u", value.ul);
				else if (type == PM_TYPE_64)
				    fprintf(stderr, " ll=%"PRIi64, value.ll);
				else if (type == PM_TYPE_U64)
				    fprintf(stderr, " ul=%"PRIu64, value.ull);
				else if (type == PM_TYPE_FLOAT)
				    fprintf(stderr, " f=%f", (double)value.f);
				else if (type == PM_TYPE_DOUBLE)
				    fprintf(stderr, " d=%f", value.d);
				else if (type == PM_TYPE_STRING)
				    fprintf(stderr, " cp=%s (len=%d)", value.cp,
						info->ivlist[k].vlen);
				else
				    fprintf(stderr, " vbp="PRINTF_P_PFX"%p (len=%d)",
						value.vbp, info->ivlist[k].vlen);
			    }
			    fputc('\n', stderr);
			    if (info != NULL)
				__dmdumpexpr(cp->mlist[m].expr, 1);
			}
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
	    if ((newvset[j] = (pmValueSet *)malloc(need)) == NULL) {
		pmNoMem("__dmpostvalueset: vset", need, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	}
	newvset[j]->pmid = vset[j]->pmid;
	newvset[j]->numval = numval;
	newvset[j]->valfmt = valfmt;
	if (numval < 0)
	    continue;

	for (i = 0; i < numval; i++) {
	    pmValueBlock	*vp;

	    if (!rewrite) {
		newvset[j]->vlist[i].inst = vset[j]->vlist[i].inst;
		if ((vset[j]->valfmt == PM_VAL_DPTR) ||
		    (vset[j]->valfmt == PM_VAL_SPTR)) {
		    need = vset[j]->vlist[i].value.pval->vlen;
		    if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			pmNoMem("__dmpostvalueset: copy value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    if (pmDebugOptions.alloc) {
			char	strbuf[20];
			fprintf(stderr, "__dmpostvalueset: pmValueBlock alloc: " PRINTF_P_PFX "%p newvset: " PRINTF_P_PFX "%p pmid: %s valfmt: %d\n",
			    vp, newvset, pmIDStr_r(vset[j]->pmid, strbuf, sizeof(strbuf)), vset[j]->valfmt);
		    }
		    memcpy((void *)vp, (void *)vset[j]->vlist[i].value.pval, need);
		    newvset[j]->vlist[i].value.pval = vp;
		    if (vset[j]->valfmt == PM_VAL_SPTR) {
			/*
			 * memcpy() means this is no longer static buffer,
			 * change valfmt so pmFreeResult() is a happy
			 * camper and there's no memory leak
			 */
			newvset[j]->valfmt = PM_VAL_DPTR;
		    }
		}
		else {
		    /* punt on vset[j]->valfmt == PM_VAL_INSITU */
		    newvset[j]->vlist[i].value.lval = vset[j]->vlist[i].value.lval;
		}
		continue;
	    }

	    /*
	     * the rewrite case ...
	     */
	    newvset[j]->vlist[i].inst = cp->mlist[m].expr->data.info->ivlist[i].inst;
	    switch (cp->mlist[m].expr->desc.type) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    newvset[j]->vlist[i].value.lval = cp->mlist[m].expr->data.info->ivlist[i].value.l;
		    break;

		case PM_TYPE_64:
		case PM_TYPE_U64:
		    need = PM_VAL_HDR_SIZE + sizeof(__int64_t);
		    if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			pmNoMem("__dmpostvalueset: 64-bit int value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = cp->mlist[m].expr->desc.type;
		    memcpy((void *)vp->vbuf, (void *)&cp->mlist[m].expr->data.info->ivlist[i].value.ll, sizeof(__int64_t));
		    newvset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_FLOAT:
		    need = PM_VAL_HDR_SIZE + sizeof(float);
		    if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			pmNoMem("__dmpostvalueset: float value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = PM_TYPE_FLOAT;
		    memcpy((void *)vp->vbuf, (void *)&cp->mlist[m].expr->data.info->ivlist[i].value.f, sizeof(float));
		    newvset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_DOUBLE:
		    need = PM_VAL_HDR_SIZE + sizeof(double);
		    if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			pmNoMem("__dmpostvalueset: double value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = PM_TYPE_DOUBLE;
		    memcpy((void *)vp->vbuf, (void *)&cp->mlist[m].expr->data.info->ivlist[i].value.f, sizeof(double));
		    newvset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_STRING:
		    need = PM_VAL_HDR_SIZE + cp->mlist[m].expr->data.info->ivlist[i].vlen;
		    if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			pmNoMem("__dmpostvalueset: string value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    vp->vlen = need;
		    vp->vtype = cp->mlist[m].expr->desc.type;
		    memcpy((void *)vp->vbuf, cp->mlist[m].expr->data.info->ivlist[i].value.cp, cp->mlist[m].expr->data.info->ivlist[i].vlen);
		    newvset[j]->vlist[i].value.pval = vp;
		    break;

		case PM_TYPE_AGGREGATE:
		case PM_TYPE_AGGREGATE_STATIC:
		case PM_TYPE_EVENT:
		case PM_TYPE_HIGHRES_EVENT:
		    need = cp->mlist[m].expr->data.info->ivlist[i].vlen;
		    if ((vp = (pmValueBlock *)malloc(need)) == NULL) {
			pmNoMem("__dmpostvalueset: aggregate or event value", need, PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    memcpy((void *)vp, cp->mlist[m].expr->data.info->ivlist[i].value.vbp, cp->mlist[m].expr->data.info->ivlist[i].vlen);
		    newvset[j]->vlist[i].value.pval = vp;
		    break;

		default:
		    /*
		     * really nothing should end up here ...
		     * do nothing as numval should have been < 0
		     */
		    if (pmDebugOptions.derive) {
			char	strbuf[20];
			fprintf(stderr, "__dmpostvalueset: botch: drived metric[%d]: operand %s has odd type (%d)\n", m, pmIDStr_r(vset[j]->pmid, strbuf, sizeof(strbuf)), cp->mlist[m].expr->desc.type);
		    }
		    break;
	    }
	}
    }

    return fails;
}

void
__dmpostfetch(__pmContext *ctxp, __pmResult **result)
{
    struct timespec	timestamp;
    __pmResult		*newrp;
    __pmResult		*rp = *result;
    ctl_t		*cp = (ctl_t *)ctxp->c_dm;
    int			fails;

    /* if needed, __dminit() called in __dmopencontext beforehand */
    if (cp == NULL || cp->fetch_has_dm == 0)
	return;

    if (pmDebugOptions.derive && pmDebugOptions.desperate) {
	fprintf(stderr, "__dmpostfetch: from context before rewrite ...\n");
	__pmPrintResult_ctx(ctxp, stderr, rp);
    }

    if ((newrp = __pmAllocResult(cp->numpmid)) == NULL) {
	pmNoMem("__dmpostfetch: newrp", sizeof(__pmResult) + (cp->numpmid - 1) * sizeof(pmValueSet *), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    newrp->numpmid = cp->numpmid;
    newrp->timestamp = rp->timestamp;

    timestamp.tv_sec = rp->timestamp.sec;
    timestamp.tv_nsec = rp->timestamp.nsec;
    fails = __dmpostvalueset(ctxp, &timestamp, rp->numpmid, rp->vset,
				newrp->numpmid, newrp->vset);
    if (fails > 0 && pmDebugOptions.derive)
	__pmPrintResult_ctx(ctxp, stderr, rp);

    /* cull the original __pmResult and return the rewritten one */
    __pmFreeResult(rp);
    *result = newrp;
}
