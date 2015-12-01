/*
 * Copyright (c) 2014-2015 Red Hat, Inc.  All Rights Reserved.
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
 */

#define _XOPEN_SOURCE 600


#include "pmapi.h"
#include "impl.h"
#include "internal.h"
#include <math.h>
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <float.h>


#ifndef LLONG_MAX
#define LLONG_MAX 9223372036854775807L
#endif
#ifndef LLONG_MIN
#define LLONG_MIN -LLONG_MAX-1L
#endif
#ifndef ULLONG_MAX
#define ULLONG_MIN 18446744073709551615ULL
#endif


/* ------------------------------------------------------------------------ */

/*
 * An individual fetch-group is a linked list of requests tied to a
 * particular pcp context (which the fetchgroup owns).  NB: This is
 * opaque to the PMAPI client.
 */
struct __pmFetchGroup
{
    int ctx;			/* our pcp context */
    pmResult *prevResult;
    struct __pmFetchGroupItem *items;
    pmID *unique_pmids;
    size_t num_unique_pmids;
};
typedef struct __pmFetchGroup *pmFG;	/* duplicate pmapi.h */


/*
 * Common data to describe value/scale conversion.
 */
struct __pmFetchGroupConversionSpec
{
    unsigned rate_convert_p:1;
    unsigned unit_convert_p:1;
    pmUnits output_units;	/* NB: same dim* as input units; maybe different scale */
    double output_multiplier;
};
typedef struct __pmFetchGroupConversionSpec *pmFGC;



/*
 * An instance of __pmFetchGroupItem stores copies of all the metadata
 * corresponding to one pmExtendFetchGroup* request.  It's organized
 * into a singly linked list, and uses a variant-union to store most
 * item-type-specific data.
 */
struct __pmFetchGroupItem
{
    struct __pmFetchGroupItem *next;
    enum
    { pmfg_item,
	pmfg_indom,
	pmfg_timestamp
    } type;

    union
    {
	struct
	{
	    pmID metric_pmid;
	    pmDesc metric_desc;
	    int metric_inst;	/* unused if metric_desc.indom == PM_INDOM_NULL */
	    struct __pmFetchGroupConversionSpec conv;
	    pmAtomValue *output_value;	/* NB: may be NULL */
	    int output_type;	/* PM_TYPE_* */
	    int *output_sts;	/* NB: may be NULL */
	} item;
	struct
	{
	    pmID metric_pmid;
	    pmDesc metric_desc;
	    int *indom_codes;	/* saved from pmGetInDom */
	    char **indom_names;
	    unsigned indom_size;
	    struct __pmFetchGroupConversionSpec conv;
	    int *output_inst_codes;	/* NB: may be NULL */
	    char **output_inst_names;	/* NB: may be NULL */
	    pmAtomValue *output_values;	/* NB: may be NULL */
	    int output_type;
	    int *output_stss;	/* NB: may be NULL */
	    int *output_sts;	/* NB: may be NULL */
	    unsigned output_maxnum;
	    unsigned *output_num;	/* NB: may be NULL */
	} indom;
	struct
	{
	    struct timeval *output_value;	/* NB: may be NULL */
	} timestamp;
    } u;
};
typedef struct __pmFetchGroupItem *pmFGI;

/* Look ma, no globals! */



/* ------------------------------------------------------------------------ */
/* Internal functions for finding, converting data. */


/*
 * Update the accumulated set of unique pmIDs sought by given pmFG, so
 * as to precalculate the data pmFetch() will need.
 */
static int
pmfg_add_pmid(pmFG pmfg, pmID pmid)
{
    size_t i;
    int sts;

    if (pmfg == NULL) {
	sts = -EINVAL;
	goto out;
    }

    for (i = 0; i < pmfg->num_unique_pmids; i++) {
	if (pmfg->unique_pmids[i] == pmid)
	    break;
    }

    if (i >= pmfg->num_unique_pmids) {	/* not found */
	pmID *new_unique_pmids = realloc(pmfg->unique_pmids, sizeof(pmID) * (pmfg->num_unique_pmids + 1));
	if (new_unique_pmids == NULL) {
	    sts = -ENOMEM;
	    goto out;
	}
	pmfg->unique_pmids = new_unique_pmids;
	pmfg->unique_pmids[pmfg->num_unique_pmids++] = pmid;
    }
    sts = 0;

out:
    return sts;
}


/*
 * Populate given pmFGI item structure based on common lookup &
 * verification for pmfg inputs.  Adjust instance profile to
 * include requested instance.
 */
static int
pmfg_lookup_item(const char *metric, const char *instance, pmFGI item)
{
    int sts;

    assert(item != NULL);
    assert(item->type == pmfg_item);

    sts = pmLookupName(1, (char **) &metric, &item->u.item.metric_pmid);
    if (sts != 1)
	goto out;
    sts = pmLookupDesc(item->u.item.metric_pmid, &item->u.item.metric_desc);
    if (sts < 0)
	goto out;

    /* Validate domain instance */
    if (((item->u.item.metric_desc.indom == PM_INDOM_NULL) && (instance != NULL))
	|| ((item->u.item.metric_desc.indom != PM_INDOM_NULL) && (instance == NULL))) {
	sts = PM_ERR_INDOM;
	goto out;
    }
    if (item->u.item.metric_desc.indom != PM_INDOM_NULL) {
	sts = pmLookupInDom(item->u.item.metric_desc.indom, instance);
	if (sts < 0)
	    goto out;
	item->u.item.metric_inst = sts;
	sts = pmAddProfile(item->u.item.metric_desc.indom, 1, &item->u.item.metric_inst);
    }

out:
    return sts;
}

/* Same for a whole indom.  Add the whole instance profile.  */
static int
pmfg_lookup_indom(const char *metric, pmFGI item)
{
    int sts;

    assert(item != NULL);
    assert(item->type == pmfg_indom);

    sts = pmLookupName(1, (char **) &metric, &item->u.indom.metric_pmid);
    if (sts != 1)
	goto out;
    sts = pmLookupDesc(item->u.indom.metric_pmid, &item->u.indom.metric_desc);
    if (sts < 0)
	goto out;

    /* As a convenience to users, we also accept non-indom'd metrics. */
    if (item->u.indom.metric_desc.indom != PM_INDOM_NULL)
	/* Add all instances; this will override any other past or future
	   piecemeal instance requests from __pmExtendFetchGroup_lookup. */
	sts = pmAddProfile(item->u.indom.metric_desc.indom, 0, NULL);

out:
    return sts;
}


/*
 * Parse & type-check the given pmDesc for conversion to given scale
 * units.  Fill in conversion specification.
 */
static int
pmfg_prep_conversion(const pmDesc * desc, const char *scale, pmFGC conv, int otype)
{
    int sts = 0;

    assert(conv != NULL);

    /* Validate type */
    switch (desc->type) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	case PM_TYPE_64:
	case PM_TYPE_U64:
	case PM_TYPE_FLOAT:
	case PM_TYPE_DOUBLE:
	case PM_TYPE_STRING:
	    break;
	default:
	    sts = PM_ERR_TYPE;
	    goto out;
    }

    /* Validate output type */
    switch (otype) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	case PM_TYPE_64:
	case PM_TYPE_U64:
	case PM_TYPE_FLOAT:
	case PM_TYPE_DOUBLE:
	case PM_TYPE_STRING:
	    break;
	default:
	    sts = PM_ERR_TYPE;
	    goto out;
    }

    /* Validate unit conversion */
    if (scale == NULL) {
	conv->rate_convert_p = (desc->sem == PM_SEM_COUNTER);
	conv->unit_convert_p = 0;
    }
    else if (strcmp(scale, "rate") == 0) {
	conv->rate_convert_p = 1;
	conv->unit_convert_p = 0;
    }
    else if (strcmp(scale, "instant") == 0) {
	conv->rate_convert_p = 0;
	conv->unit_convert_p = 0;
    }
    else {
	char *errmsg;
	sts = pmParseUnitsStr(scale, &conv->output_units, &conv->output_multiplier, &errmsg);
	if (sts < 0) {
	    /* XXX: propagate errmsg? */
	    free(errmsg);
	    goto out;
	}
	/* Allow rate-conversion, but otherwise match dimensionality. */
	if ((desc->units.dimSpace == conv->output_units.dimSpace)
	    && (desc->units.dimCount == conv->output_units.dimCount)
	    && (desc->units.dimTime == conv->output_units.dimTime)) {
	    conv->unit_convert_p = 1;
	    conv->rate_convert_p = 0;
	    sts = 0;
	    goto out;
	}
	if ((desc->units.dimSpace == conv->output_units.dimSpace)
	    && (desc->units.dimCount == conv->output_units.dimCount)
	    && (desc->units.dimTime == conv->output_units.dimTime + 1)) {
	    switch (conv->output_units.scaleTime) {	/* Adjust for rate scaling Hz->scaleTime */
		case PM_TIME_NSEC:
		    conv->output_multiplier /= 1000000000.;
		    break;
		case PM_TIME_USEC:
		    conv->output_multiplier /= 1000000.;
		    break;
		case PM_TIME_MSEC:
		    conv->output_multiplier /= 1000.;
		    break;
		case PM_TIME_SEC:
		    break;
		case PM_TIME_MIN:
		    conv->output_multiplier *= 60.;
		    break;
		case PM_TIME_HOUR:
		    conv->output_multiplier *= 3600.;
		    break;
		default:
		    sts = PM_ERR_CONV;
		    goto out;
	    }
	    conv->output_units.dimTime++;	/* Adjust back to normal dim */
	    conv->unit_convert_p = 1;
	    conv->rate_convert_p = 1;
	    sts = 0;
	    goto out;
	}
	sts = PM_ERR_CONV;
	goto out;
    }

out:
    return sts;
}


/* 
 * Extract value from pmResult interior.  Extends pmExtractValue/pmAtomStr with
 * string<->numeric conversion support.
 */
static int
__pmExtractValue2(int valfmt, const pmValue * ival, int itype, pmAtomValue * oval, int otype)
{
    int sts;

    assert(ival != NULL);
    assert(oval != NULL);

    if (itype != PM_TYPE_STRING &&	/* already checked to be a number */
	otype == PM_TYPE_STRING) {
	pmAtomValue tmp;
	enum
	{ fmt_buf_size = 40 };	/* longer than any formatted number */
	char fmt_buf[fmt_buf_size];
	sts = pmExtractValue(valfmt, ival, itype, &tmp, itype);
	if (sts < 0)
	    goto out;
	switch (itype) {
	    case PM_TYPE_32:
		snprintf(fmt_buf, fmt_buf_size, "%d", tmp.l);
		break;
	    case PM_TYPE_U32:
		snprintf(fmt_buf, fmt_buf_size, "%u", tmp.ul);
		break;
	    case PM_TYPE_64:
		snprintf(fmt_buf, fmt_buf_size, "%" PRIi64, tmp.ll);
		break;
	    case PM_TYPE_U64:
		snprintf(fmt_buf, fmt_buf_size, "%" PRIu64, tmp.ull);
		break;
	    case PM_TYPE_FLOAT:
		snprintf(fmt_buf, fmt_buf_size, "%.9e", tmp.f);
		break;
	    case PM_TYPE_DOUBLE:
		snprintf(fmt_buf, fmt_buf_size, "%.17e", tmp.d);
		break;
	    default:
		assert(0);	/* can't happen */
	}
	oval->cp = strdup(fmt_buf);
	if (oval->cp == NULL)
	    sts = -ENOMEM;
    }
    else if (itype == PM_TYPE_STRING && otype != PM_TYPE_STRING) {
	pmAtomValue tmp;
	sts = pmExtractValue(valfmt, ival, itype, &tmp, PM_TYPE_STRING);
	if (sts)
	    goto out;
	sts = __pmStringValue(tmp.cp, oval, otype);
	free(tmp.cp);
    }
    else {
	sts = pmExtractValue(valfmt, ival, itype, oval, otype);
    }

out:
    return sts;
}



/*
 * Reinitialize the given pmAtomValue, freeing up any prior dynamic content,
 * marking it "empty".
 */
void
__pmReinitValue(pmAtomValue * oval, int otype)
{
    switch (otype) {
	case PM_TYPE_32:
	    oval->l = 0;
	    break;
	case PM_TYPE_U32:
	    oval->ul = 0;
	    break;
	case PM_TYPE_64:
	    oval->ll = 0;
	    break;
	case PM_TYPE_U64:
	    oval->ull = 0;
	    break;
	case PM_TYPE_FLOAT:
	    oval->f = nanf("");
	    break;
	case PM_TYPE_DOUBLE:
	    oval->d = nan("");
	    break;
	case PM_TYPE_STRING:
	    free(oval->cp);
	    oval->cp = NULL;
	    break;
	default:
	    assert(0);		/* prevented at pmfg_prep_conversion */
    }
}


/*
 * Set the given pmAtomValue, from the incoming double value.
 * Detect overflow/sign errors similarly to pmExtractValue().
 */
int
__pmStuffDoubleValue(double val, pmAtomValue * oval, int otype)
{
    int sts = 0;

    switch (otype) {
	case PM_TYPE_32:
	    if (val > INT_MAX || val < INT_MIN)
		sts = PM_ERR_TRUNC;
	    else
		oval->l = val;
	    break;
	case PM_TYPE_U32:
	    if (val > UINT_MAX)
		sts = PM_ERR_TRUNC;
	    else if (val < 0.0)
		sts = PM_ERR_SIGN;
	    else
		oval->ul = val;
	    break;
	case PM_TYPE_64:
	    if (val > LLONG_MAX || val < LLONG_MIN)
		sts = PM_ERR_TRUNC;
	    else
		oval->ll = val;
	    break;
	case PM_TYPE_U64:
	    if (val > ULLONG_MAX)
		sts = PM_ERR_TRUNC;
	    else if (val < 0.0)
		sts = PM_ERR_SIGN;
	    else
		oval->ull = val;
	    break;
	case PM_TYPE_FLOAT:
	    if (fabs(val) > FLT_MAX)
		sts = PM_ERR_TRUNC;
	    else
		oval->f = val;
	    break;
	case PM_TYPE_DOUBLE:
	    oval->d = val;
	    break;
	case PM_TYPE_STRING:
	{
	    enum
	    { fmt_buf_size = 40 };	/* longer than any formatted number */
	    char fmt_buf[fmt_buf_size];
	    (void) snprintf(fmt_buf, sizeof(fmt_buf), "%.17e", val);
	    oval->cp = strdup(fmt_buf);
	    if (oval->cp == NULL) {
		sts = -ENOMEM;
		goto out;
	    }
	    else {
		sts = 0;
	    }
	}
	    break;
	default:
	    assert(0);		/* prevented at pmfg_prep_conversion */
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_VALUE) {
	fprintf(stderr, "__pmStuffDoubleValue: %.17e -> %s %s\n", val, pmTypeStr(otype), pmAtomStr(oval, otype));
    }
#endif

out:
    return sts;
}


static void
pmfg_reinit_timestamp(pmFGI item)
{
    assert(item != NULL);
    assert(item->type == pmfg_timestamp);

    if (item->u.timestamp.output_value)
	memset(item->u.timestamp.output_value, 0, sizeof(struct timeval));
}


static void
pmfg_reinit_item(pmFGI item)
{
    pmAtomValue *out_value;

    assert(item != NULL);
    assert(item->type == pmfg_item);

    out_value = item->u.item.output_value;
    if (item->u.item.output_value)
	__pmReinitValue(out_value, item->u.item.output_type);

    if (item->u.indom.output_sts)
	*item->u.indom.output_sts = PM_ERR_VALUE;
}


static void
pmfg_reinit_indom(pmFGI item)
{
    unsigned i;

    assert(item != NULL);
    assert(item->type == pmfg_indom);

    if (item->u.indom.output_values)
	for (i = 0; i < item->u.indom.output_maxnum; i++)
	    __pmReinitValue(&item->u.indom.output_values[i], item->u.indom.output_type);

    if (item->u.indom.output_inst_names)
	for (i = 0; i < item->u.indom.output_maxnum; i++)
	    item->u.indom.output_inst_names[i] = NULL;	/* break ref into indom_names[] */

    if (item->u.indom.output_stss)
	for (i = 0; i < item->u.indom.output_maxnum; i++)
	    item->u.indom.output_stss[i] = PM_ERR_VALUE;

    if (item->u.indom.output_num)
	*item->u.indom.output_num = 0;
}


/* 
 * Find the pmValue corresponding to the item within the given
 * pmResult.  Convert it to given output type, including possible
 * string<->number conversions.
 */
static int
pmfg_extract_item(pmID metric_pmid, int metric_inst, const pmDesc * metric_desc, const pmResult * result,
		  pmAtomValue * value, int otype)
{
    int i;

    assert(metric_desc != NULL);
    assert(result != NULL);
    assert(value != NULL);

    for (i = 0; i < result->numpmid; i++) {
	const pmValueSet *iv = result->vset[i];
	if (iv->pmid == metric_pmid) {
	    int j;
	    if (iv->numval < 0)	/* Pass error code, if any. */
		return (iv->numval);
	    for (j = 0; j < iv->numval; j++) {
		if ((metric_desc->indom == PM_INDOM_NULL) || (iv->vlist[j].inst == metric_inst)) {
		    return __pmExtractValue2(iv->valfmt, &iv->vlist[j], metric_desc->type, value, otype);
		}
	    }
	}
    }

    return PM_ERR_VALUE;
}



/*
 * Perform scaling & unit-conversion on a double.  Subsequent rate
 * conversion may be done by caller.
 */
static int
pmfg_convert_double(const pmDesc * desc, const pmFGC conv, double *value)
{
    pmAtomValue v, v_scaled;
    int sts;

    assert(value != NULL);

    if (!conv->unit_convert_p)
	return 0;

    v.d = *value;

    /* Unit conversion. */
    sts = pmConvScale(PM_TYPE_DOUBLE, &v, &desc->units, &v_scaled, &conv->output_units);
    if (sts)
	goto out;

    *value = v_scaled.d * conv->output_multiplier;

out:
    return sts;
}


static int
pmfg_extract_convert_item(pmFG pmfg, pmID metric_pmid, int metric_inst, const pmDesc * desc, const pmFGC conv,
			  const pmResult * result, pmAtomValue * oval, int otype)
{
    pmAtomValue v;
    int sts;
    double value;

    assert(result != NULL);
    assert(oval != NULL);

    sts = pmfg_extract_item(metric_pmid, metric_inst, desc, result, &v, PM_TYPE_DOUBLE);
    if (sts)
	goto out;

    if (conv->rate_convert_p) {
	if (pmfg->prevResult) {
	    pmAtomValue prev_v;
	    double deltaT = __pmtimevalSub(&result->timestamp,
					   &pmfg->prevResult->timestamp);
	    const double epsilon = 0.000001;	/* 1 us */
	    double delta;
	    if (deltaT < epsilon)	/* avoid division by zero */
		deltaT = epsilon;	/* XXX: or PM_ERR_CONV? */
	    /* XXX: deal with math_error facilities? */

	    sts = pmfg_extract_item(metric_pmid, metric_inst, desc, pmfg->prevResult, &prev_v, PM_TYPE_DOUBLE);
	    if (sts)
		goto out;

	    delta = (v.d - prev_v.d) / deltaT;
	    /* NB: the units of this delta value are: "metric_units / second",
	       something we don't represent formally with another pmUnits struct.  If
	       the requested output format was a "metric_units / hour" or other, the
	       pmfg_prep_conversion code will adjust the scalar multiplier to map
	       from /second to /hour etc. */

	    sts = pmfg_convert_double(desc, conv, &delta);
	    if (sts)
		goto out;

	    value = delta;
	}
	else {			/* no previous result */
	    sts = PM_ERR_VALUE;
	    goto out;
	}
    }
    else {			/* no rate conversion */
	sts = pmfg_convert_double(desc, conv, &v.d);
	if (sts)
	    goto out;

	value = v.d;
    }

    /* Convert the double temporary value into the gen oval/otype.
       This is similar to __pmStuffValue, except that the destination
       is a pmAtomValue struct of restricted type. */
    sts = __pmStuffDoubleValue(value, oval, otype);

out:
    return sts;
}


static void
pmfg_fetch_item(pmFG pmfg, pmFGI item, pmResult * newResult)
{
    int sts;
    pmAtomValue v;

    assert(item != NULL);
    assert(item->type == pmfg_item);
    assert(newResult != NULL);

    if (item->u.item.conv.rate_convert_p || item->u.item.conv.unit_convert_p) {
	sts =
	    pmfg_extract_convert_item(pmfg, item->u.item.metric_pmid, item->u.item.metric_inst,
				      &item->u.item.metric_desc, &item->u.item.conv, newResult, &v,
				      item->u.item.output_type);
	if (sts < 0)
	    goto out;
    }
    else {
	sts =
	    pmfg_extract_item(item->u.item.metric_pmid, item->u.item.metric_inst, &item->u.item.metric_desc, newResult,
			      &v, item->u.item.output_type);
	if (sts < 0)
	    goto out;
    }

    /* Pass the output value. */
    if (item->u.item.output_value)
	*item->u.item.output_value = v;

out:
    if (item->u.item.output_sts) {
	*item->u.item.output_sts = sts;
    }
}


static void
pmfg_fetch_timestamp(pmFG pmfg, pmFGI item, pmResult * newResult)
{
    assert(item->type == pmfg_timestamp);
    assert(newResult != NULL);
    (void) pmfg;

    if (item->u.timestamp.output_value)
	*item->u.timestamp.output_value = newResult->timestamp;
}


static void
pmfg_fetch_indom(pmFG pmfg, pmFGI item, pmResult * newResult)
{
    int sts;
    int i;
    unsigned j;
    int need_indom_refresh;
    const pmValueSet *iv;

    assert(item != NULL);
    assert(item->type == pmfg_indom);
    assert(newResult != NULL);

    /* Find our pmid in the newResult.  (If rate-converting, we'll need to find
       the corresponding pmid (and each instance) anew in the previous pmResult. */
    for (i = 0; i < newResult->numpmid; i++) {
	if (newResult->vset[i]->pmid == item->u.indom.metric_pmid)
	    break;
    }
    if (i >= newResult->numpmid) {
	sts = PM_ERR_VALUE;
	goto out;
    }
    iv = newResult->vset[i];

    /* Pass error code, if any. */
    if (iv->numval < 0) {
	sts = iv->numval;
	goto out;
    }

    /* Analyze newResult to see whether it only contains instances we
       already know.  This is unfortunately an O(N**2) operation.  It
       could be a bit faster if pmGetInDom promised to return the instances
       in sorted order, but alas. */
    need_indom_refresh = 0;
    if (item->u.indom.output_inst_names)	/* Caller interested at all? */
	for (j = 0; j < (unsigned)iv->numval; j++) {
	    unsigned k;
	    for (k = 0; k < item->u.indom.indom_size; k++)
		if (item->u.indom.indom_codes[k] == iv->vlist[j].inst)
		    break;
	    if (k >= item->u.indom.indom_size) {
		need_indom_refresh = 1;
		break;
	    }
	}
    if (need_indom_refresh) {
	free(item->u.indom.indom_codes);
	free(item->u.indom.indom_names);
	sts = pmGetInDom(item->u.indom.metric_desc.indom, &item->u.indom.indom_codes, &item->u.indom.indom_names);
	if (sts < 1) {
	    /* Need to manually clear; pmGetInDom claims they are undefined. */
	    item->u.indom.indom_codes = NULL;
	    item->u.indom.indom_names = NULL;
	    item->u.indom.indom_size = 0;
	}
	else {
	    item->u.indom.indom_size = sts;
	}
	/* NB: Even if the pmGetInDom failed, we can proceed with
	   decoding the instance values.  At worst, they won't get
	   supplied with instance names. */
	sts = 0;
    }

    /* Process each instance element in the pmValueSet.  We persevere
       in the face of per-item errors (including conversion errors),
       since we signal individual errors, except once we run out of
       output space. */
    for (j = 0; j < (unsigned)iv->numval; j++) {
	const pmValue *jv = &iv->vlist[j];
	pmAtomValue v;
	int stss = 0;

	if (j >= item->u.indom.output_maxnum) {	/* too many instances! */
	    sts = PM_ERR_TOOBIG;
	    goto out;
	}

	/* Pass the instance identifier to the user. */
	if (item->u.indom.output_inst_codes)
	    item->u.indom.output_inst_codes[j] = jv->inst;

	/* Look up the instance name for the user, searching the
	   cached pmGetIndom results. */
	if (item->u.indom.output_inst_names) {
	    unsigned k;
	    for (k = 0; k < item->u.indom.indom_size; k++)
		if (item->u.indom.indom_codes[k] == jv->inst) {
		    /* NB: copy the indom name char* by value.  The user is not
		       supposed to modify / free this pointer, or use it after a
		       a subsequent fetch or delete operation. */
		    item->u.indom.output_inst_names[j] = item->u.indom.indom_names[k];
		    break;
		}
	}

	/* Fetch & convert the actual value. */
	if (item->u.indom.conv.rate_convert_p || item->u.indom.conv.unit_convert_p) {
	    stss =
		pmfg_extract_convert_item(pmfg, item->u.indom.metric_pmid, jv->inst, &item->u.indom.metric_desc,
					  &item->u.indom.conv, newResult, &v, item->u.indom.output_type);

	    if (stss < 0)
		goto out1;
	}
	else {
	    stss =
		pmfg_extract_item(item->u.indom.metric_pmid, jv->inst, &item->u.indom.metric_desc, newResult, &v,
				  item->u.indom.output_type);

	    if (stss < 0)
		goto out1;
	}

	/* Pass the output value. */
	if (item->u.indom.output_values)
	    item->u.indom.output_values[j] = v;

out1:
	if (item->u.indom.output_stss)
	    item->u.indom.output_stss[j] = stss;
    }

    if (item->u.indom.output_num)
	*item->u.indom.output_num = j;

out:
    if (item->u.indom.output_sts)
	*item->u.indom.output_sts = sts;
}




/* ------------------------------------------------------------------------ */
/* Public functions exported in pmapi.h */


/* 
 * Create a new fetchgroup.  Take a duplicate of the incoming pcp context.
 * Return 0 & set *ptr on success.
 */
int
pmCreateFetchGroup(pmFG * ptr)
{
    int sts;
    pmFG pmfg;
    int saved_ctx = pmWhichContext();

    pmfg = calloc(1, sizeof(*pmfg));
    if (pmfg == NULL) {
	sts = -ENOMEM;
	goto out1;
    }

#if PR1129_FIXED
    sts = pmDupContext();
    if (sts < 0)
	goto out1;
    /* NB: implicitly pmUseContext()'d! */

    pmfg->ctx = sts;
#else
    pmfg->ctx = saved_ctx;
#endif

    /* Wipe clean all instances; we'll add them back incrementally as
       the fetchgroup is extended. */
    sts = pmDelProfile(PM_INDOM_NULL, 0, NULL);
    if (sts < 0)
	goto out2;

    /* Other fields may be left 0-initialized. */
    *ptr = pmfg;
    sts = 0;
    goto out;

out2:
#if PR1129_FIXED
    (void) pmDestroyContext(pmfg->ctx);
#endif
out1:
    free(pmfg);
out:
    (void) pmUseContext(saved_ctx);
    return sts;
}


/*
 * Lightly wrap pmSetMode for the context handed over to this fetchgroup.
 * Drop previous result, so rate calculations will have to be restarted.
 */
int
pmFetchGroupSetMode(pmFG pmfg, int mode, const struct timeval *when, int delta)
{
    int sts;
    int saved_ctx = pmWhichContext();

    if (pmfg == NULL) {
	sts = -EINVAL;
	goto out;
    }

    sts = pmUseContext(pmfg->ctx);
    if (sts < 0)
	goto out;

    /* Delete prevResult, since we can't use it for rate conversion across time jumps. */
    if (pmfg->prevResult)
	pmFreeResult(pmfg->prevResult);
    pmfg->prevResult = NULL;

    sts = pmSetMode(mode, when, delta);

out:
    (void) pmUseContext(saved_ctx);
    return sts;
}


/* 
 * Fetchgroup extend operations: add one metric (or a whole indom of metrics)
 * to the group.  Check types, parse rescale parameters, store away pointers
 * where results are to be written later - during a pmFetchGroup().
 */
int
pmExtendFetchGroup_item(pmFG pmfg, const char *metric, const char *instance, const char *scale, pmAtomValue * out_value,
			int out_type, int *out_sts)
{
    int sts;
    pmFGI item;
    int saved_ctx = pmWhichContext();

    if (pmfg == NULL || metric == NULL) {
	sts = -EINVAL;
	goto out;
    }

    sts = pmUseContext(pmfg->ctx);
    if (sts != 0)
	goto out;

    item = calloc(1, sizeof(*item));
    if (item == NULL) {
	sts = -ENOMEM;
	goto out;
    }

    item->type = pmfg_item;

    sts = pmfg_lookup_item(metric, instance, item);
    if (sts != 0) {
        /* If this was an archive, the instance/indom pair may not be present
           as of the current moment.  Seek to the end of the archive temporarily
           to try again there. */
        __pmContext *ctxp = __pmHandleToPtr(pmfg->ctx);
        assert(ctxp);
        if (ctxp->c_type == PM_CONTEXT_ARCHIVE) {
            struct timeval saved_origin;
            struct timeval archive_end;
            int saved_mode, saved_delta;
            saved_origin.tv_sec = ctxp->c_origin.tv_sec;
            saved_origin.tv_usec = ctxp->c_origin.tv_usec;
            saved_mode = ctxp->c_mode;
            saved_delta = ctxp->c_delta;
            PM_UNLOCK(ctxp->c_lock);
            sts = pmGetArchiveEnd(&archive_end);
            if (sts < 0)
                goto out1;
            sts = pmSetMode (PM_MODE_BACK, &archive_end, 0);
            if (sts < 0)
                goto out1;
            /* try again */
            sts = pmfg_lookup_item(metric, instance, item);
            /* go back to saved position */
            (void) pmSetMode (saved_mode, &saved_origin, saved_delta);
            if (sts < 0)
                goto out1;
        } else { /* not archive */
            PM_UNLOCK(ctxp->c_lock);
            goto out1;
        }
    }

    sts = pmfg_prep_conversion(&item->u.item.metric_desc, scale, &item->u.item.conv, out_type);
    if (sts != 0)
	goto out1;

    sts = pmfg_add_pmid(pmfg, item->u.item.metric_pmid);
    if (sts < 0)
	goto out1;

    item->u.item.output_value = out_value;
    item->u.item.output_type = out_type;
    item->u.item.output_sts = out_sts;

    /* ensure no stale pointers/content before first reinit */
    if (out_value)
	memset(out_value, 0, sizeof(*out_value));
    pmfg_reinit_item(item);

    /* link in */
    item->next = pmfg->items;
    pmfg->items = item;
    sts = 0;
    goto out;

out1:
    free(item);
out:
    (void) pmUseContext(saved_ctx);
    return sts;
}


int
pmExtendFetchGroup_timestamp(pmFG pmfg, struct timeval *out_value)
{
    int sts;
    pmFGI item;
    int saved_ctx = pmWhichContext();

    if (pmfg == NULL) {
	sts = -EINVAL;
	goto out;
    }

    item = calloc(1, sizeof(*item));
    if (item == NULL) {
	sts = -ENOMEM;
	goto out;
    }

    item->type = pmfg_timestamp;
    item->u.timestamp.output_value = out_value;

    pmfg_reinit_timestamp(item);

    /* link in */
    item->next = pmfg->items;
    pmfg->items = item;
    sts = 0;
    goto out;

out:
    (void) pmUseContext(saved_ctx);
    return sts;
}


int
pmExtendFetchGroup_indom(pmFG pmfg, const char *metric, const char *scale, int out_inst_codes[], char *out_inst_names[],
			 pmAtomValue out_values[], int out_type, int out_stss[], unsigned out_maxnum, unsigned *out_num,
			 int *out_sts)
{
    int sts;
    pmFGI item;
    int saved_ctx = pmWhichContext();

    if (pmfg == NULL || metric == NULL) {
	sts = -EINVAL;
	goto out;
    }

    sts = pmUseContext(pmfg->ctx);
    if (sts != 0)
	goto out;

    item = calloc(1, sizeof(*item));
    if (item == NULL) {
	sts = -ENOMEM;
	goto out;
    }

    item->type = pmfg_indom;

    sts = pmfg_lookup_indom(metric, item);
    if (sts != 0)
	goto out1;

    sts = pmfg_prep_conversion(&item->u.indom.metric_desc, scale, &item->u.indom.conv, out_type);
    if (sts != 0)
	goto out1;

    sts = pmfg_add_pmid(pmfg, item->u.indom.metric_pmid);
    if (sts < 0)
	goto out1;

    item->u.indom.output_inst_codes = out_inst_codes;
    item->u.indom.output_inst_names = out_inst_names;
    item->u.indom.output_values = out_values;
    item->u.indom.output_type = out_type;
    item->u.indom.output_stss = out_stss;
    item->u.indom.output_sts = out_sts;
    item->u.indom.output_maxnum = out_maxnum;
    item->u.indom.output_num = out_num;

    /* ensure no stale pointers/content before first reinit */
    if (out_values)
	memset(out_values, 0, sizeof(pmAtomValue) * out_maxnum);
    pmfg_reinit_indom(item);

    /* link in */
    item->next = pmfg->items;
    pmfg->items = item;
    sts = 0;
    goto out;

out1:
    free(item);
out:
    (void) pmUseContext(saved_ctx);
    return sts;
}


/* 
 * Call pmFetch() for the whole group.  Unpack/convert/store results
 * and error codes for all items that requested it.
 */
int
pmFetchGroup(pmFG pmfg)
{
    int sts;
    int saved_ctx = pmWhichContext();
    pmResult *newResult;
    pmFGI item;
    pmResult dummyResult;

    if (pmfg == NULL) {
	sts = -EINVAL;
	goto out;
    }

    /* Walk the fetchgroup, reinitializing every output spot, regardless of
       later errors. */
    for (item = pmfg->items; item; item = item->next) {
	switch (item->type) {
	    case pmfg_timestamp:
		pmfg_reinit_timestamp(item);
		break;
	    case pmfg_item:
		pmfg_reinit_item(item);
		break;
	    case pmfg_indom:
		pmfg_reinit_indom(item);
		break;
	    default:
		assert(0);	/* can't happen */
	}
    }

    sts = pmUseContext(pmfg->ctx);
    if (sts != 0)
	goto out;

    sts = pmFetch((int) pmfg->num_unique_pmids, pmfg->unique_pmids, &newResult);
    if (sts < 0 || newResult == NULL) {
	/* XXX: consider pmReconnectContext if PM_ERR_IPC? */
	/* XXX: is it possible to have newResult==NULL but sts >= 0? */

	/* Populate an empty fetch result, which will send out the
	   appropriate PM_ERR_VALUE etc. indications to the fetchgroup
	   items. */
	(void) gettimeofday(&dummyResult.timestamp, NULL);
	dummyResult.numpmid = 0;
	dummyResult.vset[0] = NULL;
	newResult = &dummyResult;
    }

    /* Sort the instances so that the indom fetchgroups come out conveniently. */
    pmSortInstances(newResult);

    /* Walk the fetchgroup. */
    for (item = pmfg->items; item; item = item->next) {
	switch (item->type) {
	    case pmfg_timestamp:
		pmfg_fetch_timestamp(pmfg, item, newResult);
		break;
	    case pmfg_item:
		pmfg_fetch_item(pmfg, item, newResult);
		break;
	    case pmfg_indom:
		pmfg_fetch_indom(pmfg, item, newResult);
		break;
	    default:
		assert(0);	/* can't happen */
	}
    }

    /* Store new result as previous, if there was one.  If we
       encountered a pmFetch error this time, retain the previous
       results, as a rate-conversion reference for a future successful
       pmFetch. */
    if (newResult != &dummyResult) {
	if (pmfg->prevResult)
	    pmFreeResult(pmfg->prevResult);
	pmfg->prevResult = newResult;
    }

    /* NB: we pass through the pmFetch() sts. */
out:
    (void) pmUseContext(saved_ctx);
    return sts;
}


/*
 * Destroy the fetchgroup; release all items & related dynamic data.
 */
int
pmDestroyFetchGroup(pmFG pmfg)
{
    int sts;
    int saved_ctx = pmWhichContext();
    pmFGI item;

    if (pmfg == NULL) {
	sts = -EINVAL;
	goto out;
    }

    /* Walk the items carefully since we're deleting them. */
    item = pmfg->items;
    while (item) {
	pmFGI next_item = item->next;
	switch (item->type) {
	    case pmfg_item:
		pmfg_reinit_item(item);
		break;
	    case pmfg_indom:
		pmfg_reinit_indom(item);
		free(item->u.indom.indom_codes);
		free(item->u.indom.indom_names);
		break;
	    case pmfg_timestamp:
		/* no dynamically allocated content. */
		break;
	    default:
		assert(0);	/* can't happen */
	}
	free(item);
	item = next_item;
    }

    if (pmfg->prevResult)
	pmFreeResult(pmfg->prevResult);

#if PR1129_FIXED
    (void) pmDestroyContext(pmfg->ctx);
#endif
    free(pmfg->unique_pmids);
    free(pmfg);
    sts = 0;

out:
    (void) pmUseContext(saved_ctx);
    return sts;
}
