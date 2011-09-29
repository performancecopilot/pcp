/*
 * pmResult rewrite methods for pmlogrewrite
 *
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
 */

#include "pmapi.h"
#include "impl.h"
#include "logger.h"
#include <assert.h>

/* for __pmPool* alloc */
#define MAGIC PM_VAL_HDR_SIZE + sizeof(__int64_t)

/*
 * Keep track of pmValueBlocks allocated from __pmStuffValue() during
 * rewriting.
 */
static pmValueBlock	**new_pvb = NULL;
static int		num_new_pvb = 0;
static int		next_new_pvb = 0;

static void
add_pvb(pmValueBlock *pvb)
{
    if (next_new_pvb == num_new_pvb) {
	if (num_new_pvb == 0)
	    num_new_pvb = 64;
	else
	    num_new_pvb *= 2;
	new_pvb = (pmValueBlock **)realloc(new_pvb, num_new_pvb * sizeof(new_pvb[0]));
	if (new_pvb == NULL) {
	    fprintf(stderr, "new_pvb realloc(...,%d) failed: %s\n", (int)(num_new_pvb * sizeof(new_pvb[0])), strerror(errno));
	    exit(1);
	}
    }
    new_pvb[next_new_pvb++] = pvb;
}

static void
clean_pvb(void)
{
    if (next_new_pvb > 0) {
	while (--next_new_pvb >= 0) {
	    if (new_pvb[next_new_pvb]->vlen == MAGIC)
		__pmPoolFree(new_pvb[next_new_pvb], MAGIC);
	    else
		free(new_pvb[next_new_pvb]);
	}
	next_new_pvb = 0;
    }
}

/*
 * pick/calculate one value from multiple values for the ith vset[] ...
 */
static int
pick_val(int i, metricspec_t *mp)
{
    int		j;
    int		pick = -1;

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
		switch (mp->output) {
		    case OUTPUT_MIN:
			if (*(__int64_t *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf < *(__int64_t *)inarch.rp->vset[i]->vlist[pick].value.pval->vbuf)
			    pick = j;
			break;
		    case OUTPUT_MAX:
			if (*(__int64_t *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf > *(__int64_t *)inarch.rp->vset[i]->vlist[pick].value.pval->vbuf)
			    pick = j;
			break;
		    case OUTPUT_SUM:
		    case OUTPUT_AVG:
			*(__int64_t *)inarch.rp->vset[i]->vlist[0].value.pval->vbuf += *(__int64_t *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf;
			break;
		}
		break;
	    case PM_TYPE_U64:
		switch (mp->output) {
		    case OUTPUT_MIN:
			if (*(__uint64_t *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf < *(__uint64_t *)inarch.rp->vset[i]->vlist[pick].value.pval->vbuf)
			    pick = j;
			break;
		    case OUTPUT_MAX:
			if (*(__uint64_t *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf > *(__uint64_t *)inarch.rp->vset[i]->vlist[pick].value.pval->vbuf)
			    pick = j;
			break;
		    case OUTPUT_SUM:
		    case OUTPUT_AVG:
			*(__uint64_t *)inarch.rp->vset[i]->vlist[0].value.pval->vbuf += *(__uint64_t *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf;
			break;
		}
		break;
	    case PM_TYPE_FLOAT:
		switch (mp->output) {
		    case OUTPUT_MIN:
			if (*(float *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf < *(float *)inarch.rp->vset[i]->vlist[pick].value.pval->vbuf)
			    pick = j;
			break;
		    case OUTPUT_MAX:
			if (*(float *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf > *(float *)inarch.rp->vset[i]->vlist[pick].value.pval->vbuf)
			    pick = j;
			break;
		    case OUTPUT_SUM:
		    case OUTPUT_AVG:
			*(float *)inarch.rp->vset[i]->vlist[0].value.pval->vbuf += *(float *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf;
			break;
		}
		break;
	    case PM_TYPE_DOUBLE:
		switch (mp->output) {
		    case OUTPUT_MIN:
			if (*(double *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf < *(double *)inarch.rp->vset[i]->vlist[pick].value.pval->vbuf)
			    pick = j;
			break;
		    case OUTPUT_MAX:
			if (*(double *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf > *(double *)inarch.rp->vset[i]->vlist[pick].value.pval->vbuf)
			    pick = j;
			break;
		    case OUTPUT_SUM:
		    case OUTPUT_AVG:
			*(double *)inarch.rp->vset[i]->vlist[0].value.pval->vbuf += *(double *)inarch.rp->vset[i]->vlist[j].value.pval->vbuf;
			break;
		}
		break;
	}
    }

    if (mp->output == OUTPUT_AVG) {
	switch (mp->old_desc.type) {
	    case PM_TYPE_32:
		inarch.rp->vset[i]->vlist[0].value.lval = (int)(0.5 + inarch.rp->vset[i]->vlist[0].value.lval / (double)inarch.rp->vset[j]->numval);
		break;
	    case PM_TYPE_U32:
		*(__uint32_t *)&inarch.rp->vset[i]->vlist[0].value.lval = (__uint32_t)(0.5 + *(__uint32_t *)&inarch.rp->vset[i]->vlist[0].value.lval / (double)inarch.rp->vset[j]->numval);
		break;
	    case PM_TYPE_64:
		*(__int64_t *)inarch.rp->vset[i]->vlist[0].value.pval->vbuf = (int64_t)(0.5 + *(__int64_t *)inarch.rp->vset[i]->vlist[0].value.pval->vbuf / (double)inarch.rp->vset[j]->numval);
		break;
	    case PM_TYPE_U64:
		*(__uint64_t *)inarch.rp->vset[i]->vlist[0].value.pval->vbuf = (__uint64_t)(0.5 + *(__uint64_t *)inarch.rp->vset[i]->vlist[0].value.pval->vbuf / (double)inarch.rp->vset[j]->numval);
		break;
	    case PM_TYPE_FLOAT:
		*(float *)inarch.rp->vset[i]->vlist[0].value.pval->vbuf /= inarch.rp->vset[j]->numval;
		break;
	    case PM_TYPE_DOUBLE:
		*(double *)inarch.rp->vset[i]->vlist[0].value.pval->vbuf /= inarch.rp->vset[j]->numval;
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

    sts = old_valfmt;
    for (j = 0; j < inarch.rp->vset[i]->numval; j++) {
	sts = pmExtractValue(old_valfmt, &inarch.rp->vset[i]->vlist[j], mp->old_desc.type, &ival, mp->old_desc.type);
	if (sts < 0) {
	    /*
	     * No type conversion here, so error not expected
	     */
	    fprintf(stderr, "%s: Botch: %s (%s): extracting value: %s\n",
			pmProgname, mp->old_name, pmIDStr(mp->old_desc.pmid), pmErrStr(sts));
	    __pmDumpResult(stderr, inarch.rp);
	    abandon();
	    exit(1);
	}
	sts = pmConvScale(mp->old_desc.type, &ival, &mp->old_desc.units, &oval, &mp->new_desc.units);
	if (sts < 0) {
	    /*
	     * unless the "units" are bad (and the parser is supposed to
	     * make sure this does not happen) we do not expect errors
	     * from pmConvScale()
	     */
	    fprintf(stderr, "%s: Botch: %s (%s): scale conversion from %s",
			pmProgname, mp->old_name, pmIDStr(mp->old_desc.pmid), pmUnitsStr(&mp->old_desc.units));
	    fprintf(stderr, " to %s failed: %s\n", pmUnitsStr(&mp->new_desc.units), pmErrStr(sts));
	    __pmDumpResult(stderr, inarch.rp);
	    abandon();
	    exit(1);
	}
	if (old_valfmt == PM_VAL_DPTR) {
	    /* free current pval */
	    if (inarch.rp->vset[i]->vlist[j].value.pval->vlen == MAGIC)
		__pmPoolFree(inarch.rp->vset[i]->vlist[j].value.pval, MAGIC);
	    else
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
			pmProgname, mp->old_name, pmIDStr(mp->old_desc.pmid), pmAtomStr(&oval, mp->old_desc.type), pmTypeStr(mp->old_desc.type), pmErrStr(sts));
	    __pmDumpResult(stderr, inarch.rp);
	    abandon();
	    exit(1);
	}
	if (sts == PM_VAL_DPTR) {
// TODO printf("rescale stuff -> %p (%d) %d %d \n", inarch.rp->vset[i]->vlist[j].value.pval, inarch.rp->vset[i]->vlist[j].value.pval->vtype, inarch.rp->vset[i]->vlist[j].value.pval->vlen, sts);
	    add_pvb(inarch.rp->vset[i]->vlist[j].value.pval);
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

    sts = old_valfmt;
    for (j = 0; j < inarch.rp->vset[i]->numval; j++) {
	sts = pmExtractValue(old_valfmt, &inarch.rp->vset[i]->vlist[j], mp->old_desc.type, &val, mp->new_desc.type);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: %s (%s): extracting value from type %s",
			pmProgname, mp->old_name, pmIDStr(mp->old_desc.pmid), pmTypeStr(mp->old_desc.type));
	    fprintf(stderr, " to %s: %s\n", pmTypeStr(mp->new_desc.type), pmErrStr(sts));
	    __pmDumpResult(stderr, inarch.rp);
	    abandon();
	    exit(1);
	}
	if (old_valfmt == PM_VAL_DPTR) {
	    /* free current pval */
	    if (inarch.rp->vset[i]->vlist[j].value.pval->vlen == MAGIC)
		__pmPoolFree(inarch.rp->vset[i]->vlist[j].value.pval, MAGIC);
	    else
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
			pmProgname, mp->old_name, pmIDStr(mp->old_desc.pmid), pmAtomStr(&val, mp->new_desc.type), pmTypeStr(mp->new_desc.type), pmErrStr(sts));
	    __pmDumpResult(stderr, inarch.rp);
	    abandon();
	    exit(1);
	}
	if (sts == PM_VAL_DPTR) {
// TODO printf("retype stuff -> %p (%d) %d %d \n", inarch.rp->vset[i]->vlist[j].value.pval, inarch.rp->vset[i]->vlist[j].value.pval->vtype, inarch.rp->vset[i]->vlist[j].value.pval->vlen, sts);
	    add_pvb(inarch.rp->vset[i]->vlist[j].value.pval);
	}
    }
    inarch.rp->vset[i]->valfmt = sts;
}

void
do_result(void)
{
    metricspec_t	*mp;
    int			i;
    int			sts;
    int			orig_numpmid;
    int			*orig_numval = NULL;
    long		out_offset;

    orig_numpmid = inarch.rp->numpmid;

    for (i = 0; i < inarch.rp->numpmid; i++) {
	for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	    if (inarch.rp->vset[i]->pmid != mp->old_desc.pmid)
		continue;
	    if (mp->flags == 0 && mp->ip == NULL)
		break;
	    if (mp->flags & METRIC_DELETE) {
		/* move vset[i] to end of list, shuffle lower ones up */
		int		j;
		pmValueSet	*save = inarch.rp->vset[i];
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL1)
		    fprintf(stderr, "Delete: vset[%d] for %s\n", i, pmIDStr(inarch.rp->vset[i]->pmid));
#endif
		for (j = i+1; j < inarch.rp->numpmid; j++)
		    inarch.rp->vset[j-1] = inarch.rp->vset[j];
		inarch.rp->vset[j-1] = save;
		/* one less metric to write out, process vset[i] again */
		inarch.rp->numpmid--;
		i--;
		break;
	    }
#if PCP_DEBUG
	    /*
	     * mflags that will not force any pmResult rewrite ...
	     *   METRIC_CHANGE_NAME
	     *   METRIC_CHANGE_SEM
	     */
	    if (pmDebug & DBG_TRACE_APPL1)
		fprintf(stderr, "Rewrite: vset[%d] for %s\n", i, pmIDStr(inarch.rp->vset[i]->pmid));

#endif
	    if (mp->flags & METRIC_CHANGE_PMID)
		inarch.rp->vset[i]->pmid = mp->new_desc.pmid;
	    if ((mp->flags & METRIC_CHANGE_INDOM) && inarch.rp->vset[i]->numval > 0) {
		if (mp->output != OUTPUT_ALL) {
		    /* indom non-NULL -> NULL cases */
		    int		pick = 0;
		    if (orig_numval == NULL) {
			int	j;
			orig_numval = (int *)malloc(orig_numpmid * sizeof(int));
			if (orig_numval == NULL) {
			    fprintf(stderr, "orig_numval malloc(%d) failed: %s\n", (int)(orig_numpmid * sizeof(int)), strerror(errno));
			    exit(1);
			}
			for (j = 0; j < orig_numpmid; j++)
			    orig_numval[j] = inarch.rp->vset[j]->numval;
		    }
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
		    if (pick != 0) {
			pmValue		save;
			save = inarch.rp->vset[i]->vlist[0];
			inarch.rp->vset[i]->vlist[0] = inarch.rp->vset[i]->vlist[pick];
			inarch.rp->vset[i]->vlist[pick] = save;
		    }
		    if (pick >= 0) {
			inarch.rp->vset[i]->numval = 1;
			if (mp->old_desc.indom == PM_INDOM_NULL) {
			    /* indom NULL -> non-NULL */
			    inarch.rp->vset[i]->vlist[0].inst = mp->one_inst;
			}
		    }
		    else
			inarch.rp->vset[i]->numval = 0;
		}
	    }
	    if (mp->flags & METRIC_CHANGE_UNITS) {
		if (mp->old_desc.units.dimSpace == mp->new_desc.units.dimSpace &&
		    mp->old_desc.units.dimTime == mp->new_desc.units.dimTime &&
		    mp->old_desc.units.dimCount == mp->new_desc.units.dimCount &&
		    sflag) {
		    /*
		     * dimension the same, -s on command line, so rescale
		     * values
		     */
		    rescale(i, mp);
		}
	    }
	    if (mp->flags & METRIC_CHANGE_TYPE)
		retype(i, mp);
	    if (mp->ip != NULL) {
		/* rewrite instance ids from the indom map */
		int	j;
		int	k;
		for (k = 0; k < mp->ip->numinst; k++) {
		    if (mp->ip->flags[k] & INST_CHANGE_INST) {
			for (j = 0; j < inarch.rp->vset[i]->numval; j++) {
			    if (inarch.rp->vset[i]->vlist[j].inst == mp->ip->old_inst[k]) {
				inarch.rp->vset[i]->vlist[j].inst = mp->ip->new_inst[k];
			    }
			}
		    }
		    if (mp->ip->flags[k] & INST_DELETE) {
			for (j = 0; j < inarch.rp->vset[i]->numval; j++) {
			    if (inarch.rp->vset[i]->vlist[j].inst == mp->ip->old_inst[k]) {
				j++;
				while (j < inarch.rp->vset[i]->numval) {
				    inarch.rp->vset[i]->vlist[j-1] = inarch.rp->vset[i]->vlist[j];
				    j++;
				}
				inarch.rp->vset[i]->numval--;
			    }
			}
		    }
		}
	    }
	    break;
	}
    }

    /*
     * only output numpmid == 0 case if input was a mark record
     */
    if (orig_numpmid == 0 || inarch.rp->numpmid > 0) {
	out_offset = ftell(outarch.logctl.l_mfp);
	sts = __pmEncodeResult(PDU_OVERRIDE2, inarch.rp, &inarch.logrec);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: __pmEncodeResult: %s\n",
		    pmProgname, pmErrStr(sts));
	    exit(1);
	}
	if ((sts = __pmLogPutResult(&outarch.logctl, inarch.logrec)) < 0) {
	    fprintf(stderr, "%s: Error: __pmLogPutResult: log data: %s\n",
		    pmProgname, pmErrStr(sts));
	    exit(1);
	}
	/* do not free inarch.logrec ... this is a libpcp PDU buffer */
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    struct timeval	stamp;
	    fprintf(stderr, "Log: write ");
	    stamp.tv_sec = inarch.rp->timestamp.tv_sec;
	    stamp.tv_usec = inarch.rp->timestamp.tv_usec;
	    __pmPrintStamp(stderr, &stamp);
	    fprintf(stderr, " numpmid=%d @ offset=%ld\n", inarch.rp->numpmid, out_offset);
	}
#endif
    }

    /* restore numpmid up so all vset[]s are freed */
    inarch.rp->numpmid = orig_numpmid;
    if (orig_numval != NULL) {
	/* restore numval up so all vlist[]s are freed */
	int		j;
	for (j = 0; j < orig_numpmid; j++)
	    inarch.rp->vset[j]->numval = orig_numval[j];
	free(orig_numval);
    }

    /*
     * inarch.rp contains pmValueBlock pointers into underlying PDU
     * buffer ... new_pvb[] keeps track of any new ones created by
     * rewriting
     */
    clean_pvb();
    pmFreeResult(inarch.rp);
}
