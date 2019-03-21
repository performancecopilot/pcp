/*
 * pmResult rewrite methods for pmlogrewrite
 *
 * Copyright (c) 2017 Red Hat.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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
 * pmResult rewriting is complicated because ...
 *
 * - deleting a metric involves moving all of the following rp->vset[]
 *   entries "up" one position and decrementing rp->numpmid
 * - deleting an instance involves moving all of the following
 *   rp->vset[i]->vlist[] enties "up" one position and then decrementing
 *   rp->vset[i]->numval
 * - rescaling values involves calling __pmStuffValue which modifies
 *   rp->vset[i]->vlist[j].value, and in the case of all types other than
 *   U32 or 32 this will involve an allocation for a new pmValueBlock
 *   ... we need to keep track of the previous pmValueBlock (if any) to
 *   avoid memory leaks
 * - changing type has the same implications as rescaling
 * - a single metric within a pmResult may have both rescaling and type
 *   change
 * - the initial pmResult contains pointers into a PDU buffer so the fast
 *   track case in pmFreeresult() may not release any of the pmValueSet
 *   or pmValue or pmValueBlock allocations if pmFreeResult is given a
 *   rewritten pmResult, so we modify the pmResult in place but use save[]
 *   to remember the original pmValueSet when retyping or rescaling, use
 *   orig_numval[] to remember how many pmValue instances we had had for
 *   each pmValueSet, and use orig_numpmid to remember the original numpmid
 *   value
 */

#include "pmapi.h"
#include "libpcp.h"
#include "logger.h"
#include <assert.h>

/*
 * Keep track of pmValueSets that have been moved aside to allow for
 * new values from __pmStuffValue() during rewriting.
 */
static pmValueSet	**save = NULL;
static int		len_save = 0;

/*
 * Save rp->vset[idx] in save[idx], and build a new rp->vset[idx]
 * for the number of values expected for this metric
 */
static int
save_vset(pmResult *rp, int idx)
{
    pmValueSet	*vsp;
    int		need;
    int		j;

    if (save[idx] != NULL)
	/* already done */
	return 1;

    vsp = save[idx] = rp->vset[idx];

    if (vsp->numval > 0)
	need = sizeof(pmValueSet) + (vsp->numval-1)*sizeof(pmValue);
    else
	need = sizeof(pmValueSet);
    rp->vset[idx] = (pmValueSet *)malloc(need);
    if (rp->vset[idx] == NULL) {
	fprintf(stderr, "save_vset: malloc(%d) failed: %s\n", need, strerror(errno));
	abandon();
	/*NOTREACHED*/
    }
    rp->vset[idx]->pmid = vsp->pmid;
    rp->vset[idx]->numval = vsp->numval;
    rp->vset[idx]->valfmt = vsp->valfmt;
    for (j = 0; j < vsp->numval; j++)
	rp->vset[idx]->vlist[j].inst = vsp->vlist[j].inst;
    if (pmDebugOptions.appl2) {
	fprintf(stderr, "save_vset: vset[%d] -> " PRINTF_P_PFX "%p (was " PRINTF_P_PFX "%p) pmid=%s numval=%d\n",
		idx, rp->vset[idx], save[idx], pmIDStr(rp->vset[idx]->pmid), rp->vset[idx]->numval);
    }
    return 0;
}

/*
 * free the pval for the jth instance of the ith metric
 */
static void
free_pval(pmResult *rp, int i, int j)
{
    if (pmDebugOptions.appl2) {
	fprintf(stderr, "free_pval: free(" PRINTF_P_PFX "%p) pmid=%s inst=%d\n",
	    rp->vset[i]->vlist[j].value.pval, pmIDStr(rp->vset[i]->pmid), rp->vset[i]->vlist[j].inst);
    }
    free(rp->vset[i]->vlist[j].value.pval);
}

/*
 * if a pmValueSet was saved via save_vset(), then free the newly build 
 * pmValueSet and put the old one back in place
 */
static void
clean_vset(pmResult *rp)
{
    int		i;
    int		j;

    for (i = 0; i < rp->numpmid; i++) {
	if (save[i] != NULL) {
	    if (rp->vset[i]->valfmt == PM_VAL_DPTR) {
		/* values hanging off the pval */
		for (j = 0; j < rp->vset[i]->numval; j++)
		    free_pval(rp, i, j);
	    }
	    /*
	     * we did the vset[i] allocation in save_vset(), so malloc()
	     */
	    if (pmDebugOptions.appl2) {
		fprintf(stderr, "clean_vset: free(" PRINTF_P_PFX "%p) pmValueSet pmid=%s\n",
			rp->vset[i], pmIDStr(rp->vset[i]->pmid));
	    }
	    free(rp->vset[i]);
	    rp->vset[i] = save[i];
	    save[i] = NULL;
	}
    }
}

/*
 * pick/calculate one value from multiple values for the ith vset[] ...
 *
 * Note: pval->vbuf may not have correct alignment for all data types,
 *       so memcpy() rather than assign.
 */
static int
pick_val(int i, metricspec_t *mp)
{
    int		j;
    int		pick = -1;
    pmAtomValue	jval;
    pmAtomValue	pickval;

    assert(inarch.rp->vset[i]->numval > 0);

    for (j = 0; j < inarch.rp->vset[i]->numval; j++) {
	if (mp->output == OUTPUT_ONE) {
	    if (inarch.rp->vset[i]->vlist[j].inst == mp->one_inst) {
		pick = j;
		break;
	    }
	    continue;
	}
	if (j == 0) {
	    pick = 0;
	    switch (mp->old_desc.type) {
		case PM_TYPE_64:
		    memcpy(&pickval.ll, &inarch.rp->vset[i]->vlist[0].value.pval->vbuf, sizeof(__int64_t));
		    break;
		case PM_TYPE_U64:
		    memcpy(&pickval.ull, &inarch.rp->vset[i]->vlist[0].value.pval->vbuf, sizeof(__uint64_t));
		    break;
		case PM_TYPE_FLOAT:
		    memcpy(&pickval.f, &inarch.rp->vset[i]->vlist[0].value.pval->vbuf, sizeof(float));
		    break;
		case PM_TYPE_DOUBLE:
		    memcpy(&pickval.d, &inarch.rp->vset[i]->vlist[0].value.pval->vbuf, sizeof(double));
		    break;
	    }
	    if (mp->output == OUTPUT_MIN || mp->output == OUTPUT_MAX ||
	        mp->output == OUTPUT_SUM || mp->output == OUTPUT_AVG)
		continue;
	}
	switch (mp->old_desc.type) {
	    case PM_TYPE_32:
		switch (mp->output) {
		    case OUTPUT_MIN:
			if (inarch.rp->vset[i]->vlist[j].value.lval < inarch.rp->vset[i]->vlist[pick].value.lval)
			    pick = j;
			break;
		    case OUTPUT_MAX:
			if (inarch.rp->vset[i]->vlist[j].value.lval > inarch.rp->vset[i]->vlist[pick].value.lval)
			    pick = j;
			break;
		    case OUTPUT_SUM:
		    case OUTPUT_AVG:
			inarch.rp->vset[i]->vlist[0].value.lval += inarch.rp->vset[i]->vlist[j].value.lval;
			break;
		}
		break;
	    case PM_TYPE_U32:
		switch (mp->output) {
		    case OUTPUT_MIN:
			if ((__uint32_t)inarch.rp->vset[i]->vlist[j].value.lval < (__uint32_t)inarch.rp->vset[i]->vlist[pick].value.lval)
			    pick = j;
			break;
		    case OUTPUT_MAX:
			if ((__uint32_t)inarch.rp->vset[i]->vlist[j].value.lval > (__uint32_t)inarch.rp->vset[i]->vlist[pick].value.lval)
			    pick = j;
			break;
		    case OUTPUT_SUM:
		    case OUTPUT_AVG:
			*(__uint32_t *)&inarch.rp->vset[i]->vlist[0].value.lval += (__uint32_t)inarch.rp->vset[i]->vlist[j].value.lval;
			break;
		}
		break;
	    case PM_TYPE_64:
		memcpy(&jval.ll, &inarch.rp->vset[i]->vlist[j].value.pval->vbuf, sizeof(__int64_t));
		switch (mp->output) {
		    case OUTPUT_MIN:
			if (jval.ll < pickval.ll) {
			    pickval.ll = jval.ll;
			    pick = j;
			}
			break;
		    case OUTPUT_MAX:
			if (jval.ll > pickval.ll) {
			    pickval.ll = jval.ll;
			    pick = j;
			}
			break;
		    case OUTPUT_SUM:
		    case OUTPUT_AVG:
			pickval.ll += jval.ll;
			break;
		}
		break;
	    case PM_TYPE_U64:
		memcpy(&jval.ull, &inarch.rp->vset[i]->vlist[j].value.pval->vbuf, sizeof(__int64_t));
		switch (mp->output) {
		    case OUTPUT_MIN:
			if (jval.ull < pickval.ull) {
			    pickval.ull = jval.ull;
			    pick = j;
			}
			break;
		    case OUTPUT_MAX:
			if (jval.ull > pickval.ull) {
			    pickval.ull = jval.ull;
			    pick = j;
			}
			break;
		    case OUTPUT_SUM:
		    case OUTPUT_AVG:
			pickval.ull += jval.ull;
			break;
		}
		break;
	    case PM_TYPE_FLOAT:
		memcpy(&jval.f, &inarch.rp->vset[i]->vlist[j].value.pval->vbuf, sizeof(float));
		switch (mp->output) {
		    case OUTPUT_MIN:
			if (jval.f < pickval.f) {
			    pickval.f = jval.f;
			    pick = j;
			}
			break;
		    case OUTPUT_MAX:
			if (jval.f > pickval.f) {
			    pickval.f = jval.f;
			    pick = j;
			}
			break;
		    case OUTPUT_SUM:
		    case OUTPUT_AVG:
			pickval.f += jval.f;
			break;
		}
		break;
	    case PM_TYPE_DOUBLE:
		memcpy(&jval.d, &inarch.rp->vset[i]->vlist[j].value.pval->vbuf, sizeof(double));
		switch (mp->output) {
		    case OUTPUT_MIN:
			if (jval.d < pickval.d) {
			    pickval.d = jval.d;
			    pick = j;
			}
			break;
		    case OUTPUT_MAX:
			if (jval.d > pickval.d) {
			    pickval.d = jval.d;
			    pick = j;
			}
			break;
		    case OUTPUT_SUM:
		    case OUTPUT_AVG:
			pickval.d += jval.d;
			break;
		}
		break;
	}
    }

    if (mp->output == OUTPUT_AVG) {
	switch (mp->old_desc.type) {
	    case PM_TYPE_32:
		inarch.rp->vset[i]->vlist[0].value.lval = (int)(0.5 + inarch.rp->vset[i]->vlist[0].value.lval / (double)inarch.rp->vset[i]->numval);
		break;
	    case PM_TYPE_U32:
		*(__uint32_t *)&inarch.rp->vset[i]->vlist[0].value.lval = (__uint32_t)(0.5 + *(__uint32_t *)&inarch.rp->vset[i]->vlist[0].value.lval / (double)inarch.rp->vset[i]->numval);
		break;
	    case PM_TYPE_64:
		pickval.ll = 0.5 + pickval.ll / (double)inarch.rp->vset[i]->numval;
		break;
	    case PM_TYPE_U64:
		pickval.ull = 0.5 + pickval.ull / (double)inarch.rp->vset[i]->numval;
		break;
	    case PM_TYPE_FLOAT:
		pickval.f = pickval.f / (float)inarch.rp->vset[i]->numval;
		break;
	    case PM_TYPE_DOUBLE:
		pickval.d = pickval.d / (double)inarch.rp->vset[i]->numval;
		break;
	}
    }
    if (mp->output == OUTPUT_AVG || mp->output == OUTPUT_SUM) {
	switch (mp->old_desc.type) {
	    case PM_TYPE_64:
		memcpy(&inarch.rp->vset[i]->vlist[0].value.pval->vbuf, &pickval.ll, sizeof(__int64_t));
		break;
	    case PM_TYPE_U64:
		memcpy(&inarch.rp->vset[i]->vlist[0].value.pval->vbuf, &pickval.ull, sizeof(__uint64_t));
		break;
	    case PM_TYPE_FLOAT:
		memcpy(&inarch.rp->vset[i]->vlist[0].value.pval->vbuf, &pickval.f, sizeof(float));
		break;
	    case PM_TYPE_DOUBLE:
		memcpy(&inarch.rp->vset[i]->vlist[0].value.pval->vbuf, &pickval.d, sizeof(double));
		break;
	}
    }

    return pick;
}

/*
 * rescale values for the ith vset[]
 */
static void
rescale(int i, metricspec_t *mp)
{
    int		sts;
    int		j;
    pmAtomValue	ival;
    pmAtomValue	oval;
    int		old_valfmt = inarch.rp->vset[i]->valfmt;
    int		already_saved;
    pmValueSet	*vsp;

    sts = old_valfmt;
    already_saved = save_vset(inarch.rp, i);
    if (already_saved)
	vsp = inarch.rp->vset[i];
    else
	vsp = save[i];
    for (j = 0; j < inarch.rp->vset[i]->numval; j++) {
	sts = pmExtractValue(old_valfmt, &vsp->vlist[j], mp->old_desc.type, &ival, mp->old_desc.type);
	if (sts < 0) {
	    /*
	     * No type conversion here, so error not expected
	     */
	    fprintf(stderr, "%s: Botch: %s (%s): extracting value: %s\n",
			pmGetProgname(), mp->old_name, pmIDStr(mp->old_desc.pmid), pmErrStr(sts));
	    inarch.rp->vset[i]->numval = j;
	    __pmDumpResult(stderr, inarch.rp);
	    abandon();
	    /*NOTREACHED*/
	}
	sts = pmConvScale(mp->old_desc.type, &ival, &mp->old_desc.units, &oval, &mp->new_desc.units);
	if (sts < 0) {
	    /*
	     * unless the "units" are bad (and the parser is supposed to
	     * make sure this does not happen) we do not expect errors
	     * from pmConvScale()
	     */
	    fprintf(stderr, "%s: Botch: %s (%s): scale conversion from %s",
			pmGetProgname(), mp->old_name, pmIDStr(mp->old_desc.pmid), pmUnitsStr(&mp->old_desc.units));
	    fprintf(stderr, " to %s failed: %s\n", pmUnitsStr(&mp->new_desc.units), pmErrStr(sts));
	    inarch.rp->vset[i]->numval = j;
	    __pmDumpResult(stderr, inarch.rp);
	    abandon();
	    /*NOTREACHED*/
	}
	if (already_saved && old_valfmt == PM_VAL_DPTR) {
	    /*
	     * current value uses pval that is from a previous call to
	     * __pmStuffValue() during rewriting, not a pointer into a
	     * PDU buffer
	     */
	    if (pmDebugOptions.appl2) {
		fprintf(stderr, "rescale free(" PRINTF_P_PFX "%p) pval pmid=%s inst=%d\n",
		    inarch.rp->vset[i]->vlist[j].value.pval,
		    pmIDStr(inarch.rp->vset[i]->pmid),
		    inarch.rp->vset[i]->vlist[j].inst);
	    }
	    free(inarch.rp->vset[i]->vlist[j].value.pval);
	}
	sts = __pmStuffValue(&oval, &inarch.rp->vset[i]->vlist[j], mp->old_desc.type);
	if (sts < 0) {
	    /*
	     * unless "type" is bad (which the parser is supposed to
	     * prevent) or malloc() failed, we do not expect errors from
	     * __pmStuffValue()
	     */
	    fprintf(stderr, "%s: Botch: %s (%s): stuffing value %s (type=%s) into rewritten pmResult: %s\n",
			pmGetProgname(), mp->old_name, pmIDStr(mp->old_desc.pmid), pmAtomStr(&oval, mp->old_desc.type), pmTypeStr(mp->old_desc.type), pmErrStr(sts));
	    inarch.rp->vset[i]->numval = j;
	    __pmDumpResult(stderr, inarch.rp);
	    abandon();
	    /*NOTREACHED*/
	}
    }
    inarch.rp->vset[i]->valfmt = sts;
}

/*
 * change type of values for the ith vset[] ... real chance that this will
 * fail, as some failure modes depend on the sign or size of the data
 * values found in the pmResult
 */
static void
retype(int i, metricspec_t *mp)
{
    int		sts;
    int		j;
    pmAtomValue	val;
    int		old_valfmt = inarch.rp->vset[i]->valfmt;
    int		already_saved;
    pmValueSet	*vsp;

    sts = old_valfmt;
    already_saved = save_vset(inarch.rp, i);
    if (already_saved)
	vsp = inarch.rp->vset[i];
    else
	vsp = save[i];
    for (j = 0; j < inarch.rp->vset[i]->numval; j++) {
	sts = pmExtractValue(old_valfmt, &vsp->vlist[j], mp->old_desc.type, &val, mp->new_desc.type);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: %s (%s): extracting value from type %s",
			pmGetProgname(), mp->old_name, pmIDStr(mp->old_desc.pmid), pmTypeStr(mp->old_desc.type));
	    fprintf(stderr, " to %s: %s\n", pmTypeStr(mp->new_desc.type), pmErrStr(sts));
	    inarch.rp->vset[i]->numval = j;
	    __pmDumpResult(stderr, inarch.rp);
	    abandon();
	    /*NOTREACHED*/
	}
	if (already_saved && old_valfmt == PM_VAL_DPTR) {
	    /*
	     * current value uses pval that is from a previous call to
	     * __pmStuffValue() during rewriting, not a pointer into a
	     * PDU buffer
	     */
	    if (pmDebugOptions.appl2) {
		fprintf(stderr, "retype free(" PRINTF_P_PFX "%p) pval pmid=%s inst=%d\n",
		    inarch.rp->vset[i]->vlist[j].value.pval,
		    pmIDStr(inarch.rp->vset[i]->pmid),
		    inarch.rp->vset[i]->vlist[j].inst);
	    }
	    free(inarch.rp->vset[i]->vlist[j].value.pval);
	}
	sts = __pmStuffValue(&val, &inarch.rp->vset[i]->vlist[j], mp->new_desc.type);
	if (sts < 0) {
	    /*
	     * unless "type" is bad (which the parser is supposed to
	     * prevent) or malloc() failed, we do not expect errors from
	     * __pmStuffValue()
	     */
	    fprintf(stderr, "%s: Botch: %s (%s): stuffing value %s (type=%s) into rewritten pmResult: %s\n",
			pmGetProgname(), mp->old_name, pmIDStr(mp->old_desc.pmid), pmAtomStr(&val, mp->new_desc.type), pmTypeStr(mp->new_desc.type), pmErrStr(sts));
	    inarch.rp->vset[i]->numval = j;
	    __pmDumpResult(stderr, inarch.rp);
	    abandon();
	    /*NOTREACHED*/
	}
    }
    inarch.rp->vset[i]->valfmt = sts;
}

void
do_result(void)
{
    metricspec_t	*mp;
    int			i;
    int			j;
    int			sts;
    int			orig_numpmid;
    int			*orig_numval = NULL;

    orig_numpmid = inarch.rp->numpmid;

    if (inarch.rp->numpmid > len_save) {
	/* expand save[] */
	save = (pmValueSet **)realloc(save, inarch.rp->numpmid * sizeof(save[0]));
	if (save == NULL) {
	    fprintf(stderr, "save_vset: save realloc(...,%d) failed: %s\n", (int)(inarch.rp->numpmid * sizeof(save[0])), strerror(errno));
	    abandon();
	    /*NOTREACHED*/
	}
	for (i = len_save; i < inarch.rp->numpmid; i++)
	    save[i] = NULL;
	len_save = inarch.rp->numpmid;
    }
    orig_numval = (int *)malloc(orig_numpmid * sizeof(int));
    if (orig_numval == NULL) {
	fprintf(stderr, "orig_numval malloc(%d) failed: %s\n", (int)(orig_numpmid * sizeof(int)), strerror(errno));
	abandon();
	/*NOTREACHED*/
    }
    for (i = 0; i < orig_numpmid; i++)
	orig_numval[i] = inarch.rp->vset[i]->numval;

    for (i = 0; i < inarch.rp->numpmid; i++) {
	for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	    if (inarch.rp->vset[i]->pmid != mp->old_desc.pmid)
		continue;
	    if (mp->flags == 0 && mp->ip == NULL)
		break;
	    if (mp->flags & METRIC_DELETE) {
		/* move vset[i] to end of list, shuffle lower ones up */
		pmValueSet	*vsp = inarch.rp->vset[i];
		pmValueSet	*save_vsp = save[i];
		int		save_numval;
		save_numval = orig_numval[i];
		if (pmDebugOptions.appl2)
		    fprintf(stderr, "Delete: vset[%d] for %s\n", i, pmIDStr(inarch.rp->vset[i]->pmid));
		for (j = i+1; j < inarch.rp->numpmid; j++) {
		    inarch.rp->vset[j-1] = inarch.rp->vset[j];
		    save[j-1] = save[j];
		    orig_numval[j-1] = orig_numval[j];
		}
		inarch.rp->vset[j-1] = vsp;
		save[j-1] = save_vsp;
		orig_numval[j-1] = save_numval;
		/* one less metric to write out, process vset[i] again */
		inarch.rp->numpmid--;
		i--;
		break;
	    }
	    /*
	     * mflags that will not force any pmResult rewrite ...
	     *   METRIC_CHANGE_NAME
	     *   METRIC_CHANGE_SEM
	     */
	    if (pmDebugOptions.appl2)
		fprintf(stderr, "Rewrite: vset[%d] for %s\n", i, pmIDStr(inarch.rp->vset[i]->pmid));

	    if (mp->flags & METRIC_CHANGE_PMID)
		inarch.rp->vset[i]->pmid = mp->new_desc.pmid;
	    if ((mp->flags & METRIC_CHANGE_INDOM) && inarch.rp->vset[i]->numval > 0) {
		if (mp->output != OUTPUT_ALL) {
		    /*
		     * Output only one value ...
		     * Some instance selection to be done for the following
		     * indom cases:
		     *     non-NULL -> NULL
		     *		pick one input value, singular output
		     *     NULL -> non-NULL
		     *     	only one input value, magic-up instance id for
		     *     	output value from mp->one_inst
		     *     non-NULL -> non-NULL
		     *		pick one input value base in mp->one_inst,
		     *		copy to output
		     */
		    int		pick = 0;
		    switch (mp->output) {
			case OUTPUT_FIRST:
			    break;
			case OUTPUT_LAST:
			    pick = inarch.rp->vset[i]->numval-1;
			    break;
			case OUTPUT_ONE:
			case OUTPUT_MIN:
			case OUTPUT_MAX:
			case OUTPUT_SUM:
			case OUTPUT_AVG:
			    pick = pick_val(i, mp);
			    break;
		    }
		    if (pick >= 0) {
			if (pick > 0) {
			    /* swap vlist[0] and vlist[pick] */
			    pmValue		save;
			    save = inarch.rp->vset[i]->vlist[0];
			    inarch.rp->vset[i]->vlist[0] = inarch.rp->vset[i]->vlist[pick];
			    inarch.rp->vset[i]->vlist[0].inst = inarch.rp->vset[i]->vlist[pick].inst;
			    inarch.rp->vset[i]->vlist[pick] = save;
			}
			if (mp->new_desc.indom == PM_INDOM_NULL)
			    inarch.rp->vset[i]->vlist[0].inst = PM_IN_NULL;
			else if (mp->old_desc.indom == PM_INDOM_NULL)
			    inarch.rp->vset[i]->vlist[0].inst = mp->one_inst;
			inarch.rp->vset[i]->numval = 1;
		    }
		    else
			inarch.rp->vset[i]->numval = 0;
		}
	    }
	    /*
	     * order below is deliberate ...
	     * - cull/renumber instances if needed
	     * - rescale if needed
	     * - fix type if needed
	     */
	    if (mp->ip != NULL) {
		/* rewrite/delete instance ids from the indom map */
		int	k;
		for (k = 0; k < mp->ip->numinst; k++) {
		    if (mp->ip->inst_flags[k] & INST_CHANGE_INST) {
			for (j = 0; j < inarch.rp->vset[i]->numval; j++) {
			    if (inarch.rp->vset[i]->vlist[j].inst == mp->ip->old_inst[k]) {
				inarch.rp->vset[i]->vlist[j].inst = mp->ip->new_inst[k];
			    }
			}
		    }
		    if (mp->ip->inst_flags[k] & INST_DELETE) {
			for (j = 0; j < inarch.rp->vset[i]->numval; j++) {
			    if (inarch.rp->vset[i]->vlist[j].inst == mp->ip->old_inst[k]) {
				j++;
				while (j < inarch.rp->vset[i]->numval) {
				    inarch.rp->vset[i]->vlist[j-1] = inarch.rp->vset[i]->vlist[j];
				    j++;
				}
				if (save[i] != NULL &&
				    inarch.rp->vset[i]->valfmt == PM_VAL_DPTR) {
				    /*
				     * messy case ... last instance pval is
				     * from calling __pmStuffValue() in
				     * rewriting not a pointer into the PDU
				     * buffer, so free here because
				     * clean_vset() won't find it
				     */
				    free_pval(inarch.rp, i, j-1);
				}
				inarch.rp->vset[i]->numval--;
			    }
			}
		    }
		}
	    }
	    if (mp->flags & METRIC_RESCALE) {
		/*
		 * parser already checked that dimension is unchanged,
		 * scale is different and -s on command line or RESCALE
		 * in UNITS clause of metricspec => rescale values
		 */
		rescale(i, mp);
	    }
	    if (mp->flags & METRIC_CHANGE_TYPE)
		retype(i, mp);
	    break;
	}
    }

    /*
     * only output numpmid == 0 case if input was a mark record
     */
    if (orig_numpmid == 0 || inarch.rp->numpmid > 0) {
	unsigned long	out_offset;
	unsigned long	peek_offset;
	peek_offset = __pmFtell(outarch.archctl.ac_mfp);
	sts = __pmEncodeResult(PDU_OVERRIDE2, inarch.rp, &inarch.logrec);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: __pmEncodeResult: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    abandon();
	    /*NOTREACHED*/
	}
	peek_offset += ((__pmPDUHdr *)inarch.logrec)->len - sizeof(__pmPDUHdr) + 2*sizeof(int);
	if (peek_offset > 0x7fffffff) {
	    /*
	     * data file size will exceed 2^31-1 bytes, so force
	     * volume switch
	     */
	    newvolume(outarch.archctl.ac_curvol+1);
	}
	out_offset = __pmFtell(outarch.archctl.ac_mfp);
	if ((sts = __pmLogPutResult2(&outarch.archctl, inarch.logrec)) < 0) {
	    fprintf(stderr, "%s: Error: __pmLogPutResult2: log data: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    abandon();
	    /*NOTREACHED*/
	}
	/* do not free inarch.logrec ... this is a libpcp PDU buffer */
	if (pmDebugOptions.appl0) {
	    struct timeval	stamp;
	    fprintf(stderr, "Log: write ");
	    stamp.tv_sec = inarch.rp->timestamp.tv_sec;
	    stamp.tv_usec = inarch.rp->timestamp.tv_usec;
	    pmPrintStamp(stderr, &stamp);
	    fprintf(stderr, " numpmid=%d @ offset=%ld\n", inarch.rp->numpmid, out_offset);
	}
    }

    /* restore numpmid up so all vset[]s are freed */
    inarch.rp->numpmid = orig_numpmid;
    /*
     * put pmResult back the way it was (so pmFreeResult works correctly)
     * and release any allocated memory used in the rewriting
     */
    clean_vset(inarch.rp);
    /* restore numval up so all vlist[]s are freed */
    for (i = 0; i < orig_numpmid; i++)
	inarch.rp->vset[i]->numval = orig_numval[i];
    free(orig_numval);

    pmFreeResult(inarch.rp);
}
