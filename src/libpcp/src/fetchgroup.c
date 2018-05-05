/*
 * Copyright (c) 2014-2018 Red Hat.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU Lesser General Public
 * License for more details.
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#include <math.h>
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <float.h>

#ifndef LLONG_MAX
#define LLONG_MAX LONGLONG_MAX
#endif
#ifndef LLONG_MIN
#define LLONG_MIN -LLONG_MAX-1L
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX ULONGLONG_MAX
#endif

/* ------------------------------------------------------------------------ */

/*
 * An individual fetch-group is a linked list of requests tied to a
 * particular pcp context (which the fetchgroup owns).	NB: This is
 * opaque to the PMAPI client.
 */
struct __pmFetchGroup {
    int	ctx;			/* our pcp context */
    int wrap;			/* wrap-handling flag, set at fg-create-time */
    pmResult *prevResult;
    struct __pmFetchGroupItem *items;
    pmID *unique_pmids;
    size_t num_unique_pmids;
};

/*
 * Common data to describe value/scale conversion.
 */
struct __pmFetchGroupConversionSpec {
    unsigned rate_convert : 1;
    unsigned unit_convert : 1;
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
struct __pmFetchGroupItem {
    struct __pmFetchGroupItem *next;
    enum { pmfg_item, pmfg_indom, pmfg_event, pmfg_timestamp } type;

    union {
	struct {
	    pmID metric_pmid;
	    pmDesc metric_desc;
	    int metric_inst;	/* unused if metric_desc.indom == PM_INDOM_NULL */
	    struct __pmFetchGroupConversionSpec conv;
	    pmAtomValue *output_value;	/* NB: may be NULL */
	    int output_type;	/* PM_TYPE_* */
	    int *output_sts;	/* NB: may be NULL */
	} item;
	struct {
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
	struct {
	    pmID metric_pmid;
	    pmDesc metric_desc;
	    int metric_inst;
	    pmID field_pmid;
	    pmDesc field_desc;
	    struct __pmFetchGroupConversionSpec conv;
	    struct timespec *output_times; /* NB: may be NULL */
	    pmAtomValue *output_values;	/* NB: may be NULL */
	    pmResult **unpacked_usec_events; /* NB: may be NULL */
	    pmHighResResult **unpacked_nsec_events; /* NB: may be NULL */
	    int output_type;
	    int *output_stss;	/* NB: may be NULL */
	    int *output_sts;	/* NB: may be NULL */
	    unsigned output_maxnum;
	    unsigned *output_num;	/* NB: may be NULL */
	} event;
	struct {
	    struct timeval *output_value;	/* NB: may be NULL */
	} timestamp;
    } u;
};
typedef struct __pmFetchGroupItem *pmFGI;

/* ------------------------------------------------------------------------ */
/* Internal functions for finding, converting data. */

static inline struct timespec *
pmfg_timespec_from_timeval(const struct timeval *tv, struct timespec *ts)
{
    ts->tv_sec = tv->tv_sec;
    ts->tv_nsec = tv->tv_usec * 1000;
    return ts;
}

static inline double
pmfg_timespec_delta(const struct timespec *ap, const struct timespec *bp)
{
     return (double)(ap->tv_sec - bp->tv_sec) +
	    (long double)(ap->tv_nsec - bp->tv_nsec) / (long double)1000000000;
}

/*
 * Update the accumulated set of unique pmIDs sought by given pmFG, so
 * as to precalculate the data pmFetch() will need.
 */
static int
pmfg_add_pmid(pmFG pmfg, pmID pmid)
{
    size_t i;

    if (pmfg == NULL)
	return -EINVAL;

    for (i = 0; i < pmfg->num_unique_pmids; i++) {
	if (pmfg->unique_pmids[i] == pmid)
	    break;
    }

    if (i >= pmfg->num_unique_pmids) {	/* not found */
	size_t size = sizeof(pmID) * (pmfg->num_unique_pmids + 1);
	pmID *new_unique_pmids = realloc(pmfg->unique_pmids, size);

	if (new_unique_pmids == NULL)
	    return -ENOMEM;
	pmfg->unique_pmids = new_unique_pmids;
	pmfg->unique_pmids[pmfg->num_unique_pmids++] = pmid;
    }
    return 0;
}

/*
 * Populate given pmFGI item structure based on common lookup &
 * verification for pmfg inputs.  Adjust instance profile to
 * include requested instance.  If it's a derived metric, we 
 * don't know what instance domain(s) it could involve, so we
 * clear the instance profile entirely.
 */
static int
pmfg_lookup_item(const char *metric, const char *instance, pmFGI item)
{
    int sts;

    assert(item != NULL);
    assert(item->type == pmfg_item);

    sts = pmLookupName(1, (char **)&metric, &item->u.item.metric_pmid);
    if (sts != 1)
	return sts;
    sts = pmLookupDesc(item->u.item.metric_pmid, &item->u.item.metric_desc);
    if (sts < 0)
	return sts;

    /* Validate domain instance */
    if ((item->u.item.metric_desc.indom == PM_INDOM_NULL && instance) ||
	(item->u.item.metric_desc.indom != PM_INDOM_NULL && !instance))
	return PM_ERR_INDOM;

    /* Clear instance profile if this is an opaque derived metric. */
    if (IS_DERIVED(item->u.item.metric_desc.pmid)) {
        (void) pmAddProfile(PM_INDOM_NULL, 0, NULL);
    }

    /* Add given instance to profile. */
    if (item->u.item.metric_desc.indom != PM_INDOM_NULL) {
	sts = pmLookupInDom(item->u.item.metric_desc.indom, instance);
	if (sts < 0)
	    return sts;
	item->u.item.metric_inst = sts;
        /* Moot & harmless if IS_DERIVED. */
	sts = pmAddProfile(item->u.item.metric_desc.indom, 1,
			   &item->u.item.metric_inst);
    }
    return sts;
}

/* Same for a whole indom.  Add the whole instance profile.  */
static int
pmfg_lookup_indom(const char *metric, pmFGI item)
{
    int sts;

    assert(item != NULL);
    assert(item->type == pmfg_indom);

    sts = pmLookupName(1, (char **)&metric, &item->u.indom.metric_pmid);
    if (sts != 1)
	return sts;
    sts = pmLookupDesc(item->u.indom.metric_pmid, &item->u.indom.metric_desc);
    if (sts < 0)
	return sts;

    /* Clear instance profile if this is an opaque derived metric. */
    if (IS_DERIVED(item->u.item.metric_desc.pmid)) {
        (void) pmAddProfile(PM_INDOM_NULL, 0, NULL);
    }

    /* As a convenience to users, we also accept non-indom'd metrics */
    if (item->u.indom.metric_desc.indom == PM_INDOM_NULL)
	return 0;

    /*
     * Add all instances; this will override any other past or future
     * piecemeal instance requests from __pmExtendFetchGroup_lookup.
     * Moot & harmless if IS_DERIVED.
     */
    return pmAddProfile(item->u.indom.metric_desc.indom, 0, NULL);
}

/* Same for an event field.  */
static int
pmfg_lookup_event(const char *metric, const char *instance, const char *field, pmFGI item)
{
    int sts;

    assert(item != NULL);
    assert(item->type == pmfg_event);

    sts = pmLookupName(1, (char **)&metric, &item->u.event.metric_pmid);
    if (sts != 1)
	return sts;
    sts = pmLookupDesc(item->u.event.metric_pmid, &item->u.event.metric_desc);
    if (sts < 0)
	return sts;

    /* Validate domain instance */
    if ((item->u.event.metric_desc.indom == PM_INDOM_NULL && instance) ||
	(item->u.event.metric_desc.indom != PM_INDOM_NULL && !instance))
	return PM_ERR_INDOM;
    if (item->u.event.metric_desc.indom != PM_INDOM_NULL) {
	sts = pmLookupInDom(item->u.event.metric_desc.indom, instance);
	if (sts < 0)
	    return sts;
	item->u.event.metric_inst = sts;
	sts = pmAddProfile(item->u.event.metric_desc.indom, 1,
			   &item->u.event.metric_inst);
	if (sts < 0)
	    return sts;
    }

    /* Look up event field. */
    sts = pmLookupName(1, (char **)&field, &item->u.event.field_pmid);
    if (sts != 1)
	return sts;
    sts = pmLookupDesc(item->u.event.field_pmid, &item->u.event.field_desc);
    if (sts < 0)
	return sts;
    /* We don't support event fields with their own indoms. */
    if (item->u.event.field_desc.indom != PM_INDOM_NULL)
	return PM_ERR_INDOM;

    return 0;
}

/*
 * Parse and type-check the given pmDesc for conversion to given scale
 * units.  Fill in conversion specification.
 */
static int
pmfg_prep_conversion(const pmDesc *desc, const char *scale, pmFGC conv, int otype)
{
    int sts;

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
	    return PM_ERR_TYPE;
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
	    return PM_ERR_TYPE;
    }

    /* Validate unit conversion */
    if (scale == NULL) {
	conv->rate_convert = (desc->sem == PM_SEM_COUNTER);
	conv->unit_convert = 0;
    }
    else if (strcmp(scale, "rate") == 0) {
	conv->rate_convert = 1;
	conv->unit_convert = 0;
    }
    else if (strcmp(scale, "instant") == 0) {
	conv->rate_convert = 0;
	conv->unit_convert = 0;
    }
    else {
	char *unused;	/* discard error message */

	sts = pmParseUnitsStr(scale, &conv->output_units,
				&conv->output_multiplier, &unused);
	if (sts < 0) {
	    free(unused);
	    return sts;
	}
	/* Allow rate-conversion, but otherwise match dimensionality. */
	if (desc->units.dimSpace == conv->output_units.dimSpace &&
	    desc->units.dimCount == conv->output_units.dimCount &&
	    desc->units.dimTime == conv->output_units.dimTime) {
	    conv->unit_convert = 1;
	    conv->rate_convert = 0;
	    return 0;
	}
	if (desc->units.dimSpace == conv->output_units.dimSpace &&
	    desc->units.dimCount == conv->output_units.dimCount &&
	    desc->units.dimTime == conv->output_units.dimTime + 1) {
	    /* Adjust for rate scaling Hz->scaleTime */
	    switch (conv->output_units.scaleTime) {
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
		    return PM_ERR_CONV;
	    }
	    conv->output_units.dimTime++;	/* Adjust back to normal dim */
	    conv->unit_convert = 1;
	    conv->rate_convert = 1;
	    return 0;
	}
	return PM_ERR_CONV;
    }

    return 0;
}

/*
 * Extract value from pmResult interior.  Extends pmExtractValue/pmAtomStr with
 * string<->numeric conversion support.
 */
static int
__pmExtractValue2(int valfmt, const pmValue *ival, int itype, pmAtomValue *oval, int otype)
{
    int sts;

    assert(ival != NULL);
    assert(oval != NULL);

    if (itype != PM_TYPE_STRING &&	/* already checked to be a number */
	otype == PM_TYPE_STRING) {
	pmAtomValue tmp;
	enum { fmt_buf_size = 40 };	/* longer than any formatted number */
	char fmt_buf[fmt_buf_size];

	sts = pmExtractValue(valfmt, ival, itype, &tmp, itype);
	if (sts < 0)
	    return sts;
	switch (itype) {
	    case PM_TYPE_32:
		pmsprintf(fmt_buf, fmt_buf_size, "%d", tmp.l);
		break;
	    case PM_TYPE_U32:
		pmsprintf(fmt_buf, fmt_buf_size, "%u", tmp.ul);
		break;
	    case PM_TYPE_64:
		pmsprintf(fmt_buf, fmt_buf_size, "%" PRIi64, tmp.ll);
		break;
	    case PM_TYPE_U64:
		pmsprintf(fmt_buf, fmt_buf_size, "%" PRIu64, tmp.ull);
		break;
	    case PM_TYPE_FLOAT:
		pmsprintf(fmt_buf, fmt_buf_size, "%.9e", tmp.f);
		break;
	    case PM_TYPE_DOUBLE:
		pmsprintf(fmt_buf, fmt_buf_size, "%.17e", tmp.d);
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
	    return sts;
	sts = __pmStringValue(tmp.cp, oval, otype);
	free(tmp.cp);
    }
    else {
	sts = pmExtractValue(valfmt, ival, itype, oval, otype);
    }

    return sts;
}

/*
 * Reinitialize the given pmAtomValue, freeing up any prior dynamic content,
 * marking it "empty".
 */
void
__pmReinitValue(pmAtomValue *oval, int otype)
{
    switch (otype) {
	case PM_TYPE_FLOAT:
	    oval->f = (float)0.0 / (float)0.0; /* nanf(""); */
	    break;
	case PM_TYPE_DOUBLE:
	    oval->d = (double)0.0 / (double)0.0; /* nan(""); */
	    break;
	case PM_TYPE_STRING:
	    free(oval->cp);
	    oval->cp = NULL;
	    break;
	case PM_TYPE_32:
	case PM_TYPE_U32:
	case PM_TYPE_64:
	case PM_TYPE_U64:
	default:
	    memset(oval, -1, sizeof(*oval));
	    break;
    }
}

/*
 * Set the given pmAtomValue, from the incoming double value.
 * Detect overflow/sign errors similarly to pmExtractValue().
 */
int
__pmStuffDoubleValue(double val, pmAtomValue *oval, int otype)
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
	case PM_TYPE_STRING: {
	    enum { fmt_buf_size = 40 };	/* longer than any formatted number */
	    char fmt_buf[fmt_buf_size];

	    pmsprintf(fmt_buf, sizeof(fmt_buf), "%.17e", val);
	    oval->cp = strdup(fmt_buf);
	    if (oval->cp == NULL)
		return -ENOMEM;
	    sts = 0;
	    break;
	}
	default:
	    assert(0);		/* prevented at pmfg_prep_conversion */
    }

    if (pmDebugOptions.value) {
	fprintf(stderr, "__pmStuffDoubleValue: %.17e -> %s %s\n",
		val, pmTypeStr(otype), pmAtomStr(oval, otype));
    }
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

static void
pmfg_reinit_event(pmFGI item)
{
    unsigned i;

    assert(item != NULL);
    assert(item->type == pmfg_event);

    if (item->u.event.output_values)
	for (i = 0; i < item->u.event.output_maxnum; i++)
	    __pmReinitValue(&item->u.event.output_values[i],
			    item->u.event.output_type);

    if (item->u.event.output_times)
	for (i = 0; i < item->u.event.output_maxnum; i++)
	    memset(& item->u.event.output_times[i], 0, sizeof(struct timespec));

    if (item->u.event.output_stss)
	for (i = 0; i < item->u.event.output_maxnum; i++)
	    item->u.event.output_stss[i] = PM_ERR_VALUE;

    if (item->u.event.output_num)
	*item->u.event.output_num = 0;

    if (item->u.event.unpacked_nsec_events) {
	pmFreeHighResEventResult(item->u.event.unpacked_nsec_events);
	item->u.event.unpacked_nsec_events = NULL;
    }
    if (item->u.event.unpacked_usec_events) {
	pmFreeEventResult(item->u.event.unpacked_usec_events);
	item->u.event.unpacked_usec_events = NULL;
    }
}

/*
 * Find the pmValue corresponding to the item within the given
 * pmResult.  Convert it to given output type, including possible
 * string<->number conversions.
 */
static int
pmfg_extract_item(pmID metric_pmid, int metric_inst, int first_vset,
		  const pmDesc *metric_desc, pmValueSet **vsets,
		  int numpmid, pmAtomValue *value, int otype)
{
    int i;

    assert(metric_desc != NULL);
    assert(vsets != NULL);
    assert(value != NULL);

    for (i = first_vset; i < numpmid; i++) {
	const pmValueSet *iv = vsets[i];
	if (iv->pmid == metric_pmid) {
	    int j;

	    if (iv->numval < 0)	/* Pass error code, if any. */
		return iv->numval;
	    for (j = 0; j < iv->numval; j++) {
		if (metric_desc->indom == PM_INDOM_NULL ||
		    iv->vlist[j].inst == metric_inst)
		    return __pmExtractValue2(iv->valfmt, &iv->vlist[j],
					metric_desc->type, value, otype);
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
pmfg_convert_double(const pmDesc *desc, const pmFGC conv, double *value)
{
    pmAtomValue v, v_scaled;
    int sts;

    assert(value != NULL);

    if (!conv->unit_convert)
	return 0;

    v.d = *value;

    /* Unit conversion. */
    sts = pmConvScale(PM_TYPE_DOUBLE, &v, &desc->units,
			&v_scaled, &conv->output_units);
    if (sts)
	return sts;

    *value = v_scaled.d * conv->output_multiplier;
    return sts;
}

static int
pmfg_unwrap_counter(pmFG pmfg, int type, double *value)
{
    if (pmfg->wrap) {
	switch (type) {
	    case PM_TYPE_32:
	    case PM_TYPE_U32:
		*value += (double)UINT_MAX+1;
		break;
	    case PM_TYPE_64:
	    case PM_TYPE_U64:
		*value += (double)ULONGLONG_MAX+1;
		break;
	}
	return 0;
    }
    return PM_ERR_VALUE;
}

static int
pmfg_extract_convert_item(pmFG pmfg, pmID metric_pmid, int metric_inst,
			  int first_vset, const pmDesc *desc, const pmFGC conv,
			  pmValueSet **vsets, int numpmid,
			  const struct timespec *timestamp,
			  pmAtomValue *oval, int otype)
{
    pmAtomValue v;
    double value;
    int sts;

    assert(oval != NULL);

    sts = pmfg_extract_item(metric_pmid, metric_inst, first_vset, desc,
			    vsets, numpmid, &v, PM_TYPE_DOUBLE);
    if (sts)
	return sts;

    if (conv->rate_convert) {
	if (pmfg->prevResult) {
	    pmResult *prev_r;
	    pmAtomValue prev_v;
	    struct timespec prev_t;
	    double deltaT, delta;
	    const double epsilon = 0.000000001;	/* 1 nanosecond */

	    prev_r = pmfg->prevResult;
	    pmfg_timespec_from_timeval(&prev_r->timestamp, &prev_t);
	    deltaT = pmfg_timespec_delta(timestamp, &prev_t);

	    if (deltaT < epsilon)	/* avoid division by zero */
		deltaT = epsilon;	/* (chose not to PM_ERR_CONV here) */

	    sts = pmfg_extract_item(metric_pmid, metric_inst, first_vset,
				    desc, prev_r->vset, prev_r->numpmid,
				    &prev_v, PM_TYPE_DOUBLE);
	    if (sts)
		return sts;

	    delta = v.d - prev_v.d;
	    if (delta < 0.0)
		sts = pmfg_unwrap_counter(pmfg, desc->type, &delta);
	    /*
	     * NB: the units of this delta value are: "metric_units / second",
	     * something we don't represent formally with another pmUnits
	     * struct.
	     * If the requested output format was a "metric_units / hour" or
	     * other, the pmfg_prep_conversion code will adjust the scalar
	     * multiplier to map from /second to /hour etc.
	     */
	    if (sts == 0) {
		delta /= deltaT;
		sts = pmfg_convert_double(desc, conv, &delta);
	    }
	    if (sts)
		return sts;

	    value = delta;
	}
	else {			/* no previous result */
	    return PM_ERR_AGAIN;
	}
    }
    else {			/* no rate conversion */
	sts = pmfg_convert_double(desc, conv, &v.d);
	if (sts)
	    return sts;
	value = v.d;
    }

    /*
     * Convert the double temporary value into the gen oval/otype.
     * This is similar to __pmStuffValue, except that the destination
     * is a pmAtomValue struct of restricted type.
     */
    return __pmStuffDoubleValue(value, oval, otype);
}

static void
pmfg_fetch_item(pmFG pmfg, pmFGI item, pmResult *newResult)
{
    int sts;
    pmAtomValue v;
    int i;

    assert(item != NULL);
    assert(item->type == pmfg_item);
    assert(newResult != NULL);

    /*
     * If we have some values, then DISCRETE preserved values should
     * be cleared now.
     */
    if (item->u.item.metric_desc.sem == PM_SEM_DISCRETE) {
	for (i = 0; i < newResult->numpmid; i++) {
	    if (newResult->vset[i]->pmid == item->u.item.metric_pmid) {
		if (newResult->vset[i]->numval > 0) {
		    pmfg_reinit_item(item);
		    break;
		} else if (newResult->vset[i]->numval == 0) {
		    return; /* NB: leave outputs alone. */
		}
	    }
	}
    }

    if (item->u.item.conv.rate_convert || item->u.item.conv.unit_convert) {
	struct timespec timestamp;

	pmfg_timespec_from_timeval(&newResult->timestamp, &timestamp),
	sts = pmfg_extract_convert_item(pmfg,
			item->u.item.metric_pmid, item->u.item.metric_inst, 0,
		 	&item->u.item.metric_desc, &item->u.item.conv,
			newResult->vset, newResult->numpmid, &timestamp,
			&v, item->u.item.output_type);
	if (sts < 0)
	    goto out;
    }
    else {
	sts = pmfg_extract_item(item->u.item.metric_pmid,
			item->u.item.metric_inst, 0,
			&item->u.item.metric_desc,
			newResult->vset, newResult->numpmid,
			&v, item->u.item.output_type);
	if (sts < 0)
	    goto out;
    }

    /* Pass the output value. */
    if (item->u.item.output_value)
	*item->u.item.output_value = v;

out:
    if (item->u.item.output_sts)
	*item->u.item.output_sts = sts;
}

static void
pmfg_fetch_timestamp(pmFG pmfg, pmFGI item, pmResult *newResult)
{
    assert(item->type == pmfg_timestamp);
    assert(newResult != NULL);
    (void) pmfg;

    if (item->u.timestamp.output_value)
	*item->u.timestamp.output_value = newResult->timestamp;
}

static void
pmfg_fetch_indom(pmFG pmfg, pmFGI item, pmResult *newResult)
{
    int sts = 0;
    int i;
    unsigned j;
    int need_indom_refresh;
    const pmValueSet *iv;

    assert(item != NULL);
    assert(item->type == pmfg_indom);
    assert(newResult != NULL);

    /*
     * Find our pmid in the newResult.	If rate-converting, we'll need to
     * find the corresponding pmid (and each instance) anew in the previous
     * pmResult.
     */
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

    /*
     * If we have some values, the DISCRETE preserved values should be
     * cleared now.
     */
    if (item->u.indom.metric_desc.sem == PM_SEM_DISCRETE) {
	if (iv->numval > 0)
	    pmfg_reinit_indom(item);
	else /* = 0 */
	    return; /* NB: leave outputs alone. */
    }

    /*
     * Analyze newResult to see whether it only contains instances we
     * already know.  This is unfortunately an O(N**2) operation.  It
     * could be made a bit faster if we build a pmGetInDom()- variant
     * that provides instances in sorted order.
     */
    need_indom_refresh = 0;
    if (item->u.indom.output_inst_names) {	/* Caller interested at all? */
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
    }
    if (need_indom_refresh) {
	free(item->u.indom.indom_codes);
	free(item->u.indom.indom_names);
	sts = pmGetInDom(item->u.indom.metric_desc.indom,
			&item->u.indom.indom_codes, &item->u.indom.indom_names);
	if (sts < 1) {
	    /* Need to manually clear; pmGetInDom claims they are undefined. */
	    item->u.indom.indom_codes = NULL;
	    item->u.indom.indom_names = NULL;
	    item->u.indom.indom_size = 0;
	}
	else {
	    item->u.indom.indom_size = sts;
	}
	/*
	 * NB: Even if the pmGetInDom failed, we can proceed with
	 * decoding the instance values.  At worst, they won't get
	 * supplied with instance names.
	 */
	sts = 0;
    }

    /*
     * Process each instance element in the pmValueSet.	 We persevere
     * in the face of per-item errors (including conversion errors),
     * since we signal individual errors, except once we run out of
     * output space.
     */
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

	/*
	 * Look up the instance name for the user, searching the cached
	 * results from pmGetIndom.
	 */
	if (item->u.indom.output_inst_names) {
	    unsigned k;

	    for (k = 0; k < item->u.indom.indom_size; k++) {
		if (item->u.indom.indom_codes[k] == jv->inst) {
		    /*
		     * NB: copy the indom name char* by value.
		     * The user is not supposed to modify / free this pointer,
		     * or use it after a subsequent fetch or delete operation.
		     */
		    item->u.indom.output_inst_names[j] =
				item->u.indom.indom_names[k];
		    break;
		}
	    }
	}

	/* Fetch & convert the actual value. */
	if (item->u.indom.conv.rate_convert ||
	    item->u.indom.conv.unit_convert) {
	    struct timespec timestamp;

	    pmfg_timespec_from_timeval(&newResult->timestamp, &timestamp);
	    stss = pmfg_extract_convert_item(pmfg, item->u.indom.metric_pmid,
				jv->inst, 0, &item->u.indom.metric_desc,
				&item->u.indom.conv,
				newResult->vset, newResult->numpmid, &timestamp,
				&v, item->u.indom.output_type);
	    if (stss < 0)
		goto out1;
	}
	else {
	    stss = pmfg_extract_item(item->u.indom.metric_pmid, jv->inst,
				0, &item->u.indom.metric_desc,
				newResult->vset, newResult->numpmid, &v,
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

static int
pmfg_fetch_event_field(pmFG pmfg, pmFGI item, unsigned int *output_num,
		pmValueSet **vsets, int numpmid, const struct timespec *timestamp)
{
    int i;
    unsigned int count = *output_num;

    for (i = 0; i < numpmid; i++) {
	const pmValueSet *field = vsets[i];
	pmAtomValue v;
	int stss = 0;

	/*
	 * Is this our field of interest?
	 * NB: it may be repeated, if the PMDA emits the same
	 * field-name/pmid multiple times within the event records.
	 * See e.g. systemd.journal.records which repeats
	 * systemd.journal.field.string.
	 * These have multiple pmValueSet numval=1 records.
	 * (Hypothetically, we could observe a pmValueSet with
	 * numval=N instead, with each pmValue having the same
	 * instance number, but that does not seem to happen in the
	 * wild.)
	 */
	if (field->pmid != item->u.event.field_pmid)
	    continue;
	if (field->numval != 1)
	    continue;

	/* Got one! */
	if (count >= item->u.event.output_maxnum)	/* too many! */
	    return PM_ERR_TOOBIG;

	/*
	 * Fetch and convert the actual value.
	 * We pass -1 as the instance code, as an unused value, since
	 * the field_desc.indom == PM_INDOM_NULL already.
	 * We pass `i' as the first_vset parameter, to skip prior
	 * instances of the same field/pmid in the same pmResult.
	 */
	if (item->u.event.conv.rate_convert ||
	    item->u.event.conv.unit_convert) {
	    stss = pmfg_extract_convert_item(pmfg,
				item->u.event.field_pmid, -1, i,
				&item->u.event.field_desc, &item->u.event.conv,
				vsets, numpmid, timestamp,
				&v, item->u.event.output_type);
	    if (stss < 0)
		goto out;
	}
	else {
	    stss = pmfg_extract_item(item->u.event.field_pmid, -1,
				i, &item->u.event.field_desc, vsets, numpmid,
				&v, item->u.event.output_type);
	    if (stss < 0)
		goto out;
	}

	/* Pass the output value. */
	if (item->u.event.output_values)
	    item->u.event.output_values[count] = v;

	/* Pass the output timestamp. */
	if (item->u.event.output_times) {
	    item->u.event.output_times[count].tv_sec = timestamp->tv_sec;
	    item->u.event.output_times[count].tv_nsec = timestamp->tv_nsec;
        }

    out:
	if (item->u.event.output_stss)
	    item->u.event.output_stss[count] = stss;

	*output_num = ++count;
    }

    return 0;
}

static void
pmfg_fetch_event(pmFG pmfg, pmFGI item, pmResult *newResult)
{
    int sts = 0;
    int i;
    pmValueSet *iv;
    unsigned output_num = 0; /* to be copied into caller space at end */

    assert(item != NULL);
    assert(item->type == pmfg_event);
    assert(newResult != NULL);

    /* Find our pmid in the newResult. */
    for (i = 0; i < newResult->numpmid; i++) {
	if (newResult->vset[i]->pmid == item->u.event.metric_pmid)
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

    /* Locate the event record for the requested instance (if any). */
    for (i = 0; i < iv->numval; i++) {
	if ((item->u.event.metric_desc.indom == PM_INDOM_NULL) ||
	    (iv->vlist[i].inst == item->u.event.metric_inst))
	    break;
    }
    if (i >= iv->numval) {
	sts = 0; /* No events => no problem. */
	goto out;
    }

    /* Unpack the event records. */
    if (item->u.event.metric_desc.type == PM_TYPE_HIGHRES_EVENT) {
	assert(item->u.event.unpacked_nsec_events == NULL);
	sts = pmUnpackHighResEventRecords(iv, i,
				&item->u.event.unpacked_nsec_events);
	if (sts < 0 || item->u.event.unpacked_nsec_events == NULL)
	    goto out;
    }
    else {
	assert(item->u.event.metric_desc.type == PM_TYPE_EVENT);
	assert(item->u.event.unpacked_usec_events == NULL);
	sts = pmUnpackEventRecords(iv, i,
				&item->u.event.unpacked_usec_events);
	if (sts < 0 || item->u.event.unpacked_usec_events == NULL)
	    goto out;
    }

    /*
     * Process each event record, and each matching field within it.
     * We persevere in the face of per-record and per-field errors
     * (including conversion errors), since we signal individual
     * errors, except once we run out of output space.
     */
    sts = 0;

    if (item->u.event.metric_desc.type == PM_TYPE_HIGHRES_EVENT) {
	pmHighResResult **nresp, *event;

	for (nresp = item->u.event.unpacked_nsec_events; *nresp; nresp++) {
	    event = *nresp;
	    sts = pmfg_fetch_event_field(pmfg, item, &output_num,
			event->vset, event->numpmid, &event->timestamp);
	    if (sts < 0)
		goto out;
	}
    }
    else {
	assert(item->u.event.metric_desc.type == PM_TYPE_EVENT);
	struct timespec timestamp;
	pmResult **uresp, *event;

	for (uresp = item->u.event.unpacked_usec_events; *uresp; uresp++) {
	    event = *uresp;
	    pmfg_timespec_from_timeval(&event->timestamp, &timestamp);
	    sts = pmfg_fetch_event_field(pmfg, item, &output_num,
			event->vset, event->numpmid, &timestamp);
	    if (sts < 0)
		goto out;
	}
    }

out:
    if (item->u.event.output_num)
	*item->u.event.output_num = output_num;

    if (item->u.event.output_sts)
	*item->u.event.output_sts = sts;
}

static int
pmfg_clear_profile(pmFG pmfg)
{
    int sts;

    sts = pmUseContext(pmfg->ctx);
    if (sts != 0)
	return sts;

    /*
     * Wipe clean all instances; we'll add them back incrementally as
     * the fetchgroup is extended.  This cannot fail for the manner in
     * which we call it - see pmDelProfile(3) man page discussion.
     */
    return pmDelProfile(PM_INDOM_NULL, 0, NULL);
}


/* ------------------------------------------------------------------------ */
/* Public functions exported from libpcp and in pmapi.h */

/*
 * Create a new fetchgroup.  Take a duplicate of the incoming pcp context.
 * Return 0 and set *ptr on success.
 */
int
pmCreateFetchGroup(pmFG *ptr, int type, const char *name)
{
    int sts;
    pmFG pmfg;

    if (ptr == NULL)
	return -EINVAL;
    *ptr = NULL; /* preset it to NULL */

    pmfg = calloc(1, sizeof(*pmfg));
    if (pmfg == NULL)
	return -ENOMEM;

    sts = pmNewContext(type, name);
    if (sts < 0) {
	free(pmfg);
	return sts;
    }
    pmfg->ctx = sts;

    /* PCP_COUNTER_WRAP in environment enables "counter wrap" logic */
    PM_LOCK(__pmLock_extcall);
    if (getenv("PCP_COUNTER_WRAP") != NULL)
	pmfg->wrap = 1;
    PM_UNLOCK(__pmLock_extcall);

    pmfg_clear_profile(pmfg);

    *ptr = pmfg;
    return 0;
}

/*
 * Return our private context.	Caveat emptor!
 */
int
pmGetFetchGroupContext(pmFG pmfg)
{
    if (pmfg == NULL)
	return -EINVAL;

    return pmfg->ctx;
}

/*
 * Fetchgroup extend operations: add one metric (or a whole indom of metrics)
 * to the group.  Check types, parse rescale parameters, store away pointers
 * where results are to be written later - during a pmFetchGroup().
 */
int
pmExtendFetchGroup_item(pmFG pmfg,
			const char *metric, const char *instance,
			const char *scale, pmAtomValue *out_value,
			int out_type, int *out_sts)
{
    int sts;
    pmFGI item;

    if (pmfg == NULL || metric == NULL)
	return -EINVAL;

    sts = pmUseContext(pmfg->ctx);
    if (sts != 0)
	return sts;

    item = calloc(1, sizeof(*item));
    if (item == NULL)
	return -ENOMEM;

    item->type = pmfg_item;

    sts = pmfg_lookup_item(metric, instance, item);
    if (sts != 0) {
	/*
	 * If this is an archive, the instance/indom pair may not be
	 * present as of the current moment.  Seek to the end of the
	 * archive temporarily to try again there.
	 */
	__pmContext *ctxp = __pmHandleToPtr(pmfg->ctx);

	if (ctxp->c_type == PM_CONTEXT_ARCHIVE) {
	    struct timeval saved_origin;
	    struct timeval archive_end;
	    int saved_mode, saved_delta;

	    saved_origin.tv_sec = ctxp->c_origin.tv_sec;
	    saved_origin.tv_usec = ctxp->c_origin.tv_usec;
	    saved_mode = ctxp->c_mode;
	    saved_delta = ctxp->c_delta;
	    sts = pmGetArchiveEnd_ctx(ctxp, &archive_end);
	    PM_UNLOCK(ctxp->c_lock);
	    if (sts < 0)
		goto out;
	    sts = pmSetMode(PM_MODE_BACK, &archive_end, 0);
	    if (sts < 0)
		goto out;
	    /* try again */
	    sts = pmfg_lookup_item(metric, instance, item);
	    /* go back to saved position */
	    pmSetMode(saved_mode, &saved_origin, saved_delta);
	    if (sts < 0)
		goto out;
	} else { /* not an archive */
	    PM_UNLOCK(ctxp->c_lock);
	    goto out;
	}
    }

    sts = pmfg_prep_conversion(&item->u.item.metric_desc, scale,
				&item->u.item.conv, out_type);
    if (sts != 0)
	goto out;

    sts = pmfg_add_pmid(pmfg, item->u.item.metric_pmid);
    if (sts != 0)
	goto out;

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

    return 0;

out:
    free(item);

    return sts;
}

int
pmExtendFetchGroup_timestamp(pmFG pmfg, struct timeval *out_value)
{
    pmFGI item;

    if (pmfg == NULL)
	return -EINVAL;

    item = calloc(1, sizeof(*item));
    if (item == NULL)
	return -ENOMEM;

    item->type = pmfg_timestamp;
    item->u.timestamp.output_value = out_value;

    pmfg_reinit_timestamp(item);

    /* link in */
    item->next = pmfg->items;
    pmfg->items = item;

    return 0;
}

int
pmExtendFetchGroup_indom(pmFG pmfg,
		const char *metric, const char *scale,
		int out_inst_codes[], char *out_inst_names[],
		pmAtomValue out_values[], int out_type,
		int out_stss[], unsigned int out_maxnum,
		unsigned int *out_num, int *out_sts)
{
    int sts;
    pmFGI item;

    if (pmfg == NULL || metric == NULL)
	return -EINVAL;

    sts = pmUseContext(pmfg->ctx);
    if (sts != 0)
	return sts;

    item = calloc(1, sizeof(*item));
    if (item == NULL)
	return -ENOMEM;

    item->type = pmfg_indom;

    sts = pmfg_lookup_indom(metric, item);
    if (sts != 0)
	goto out;

    sts = pmfg_prep_conversion(&item->u.indom.metric_desc, scale,
				&item->u.indom.conv, out_type);
    if (sts != 0)
	goto out;

    sts = pmfg_add_pmid(pmfg, item->u.indom.metric_pmid);
    if (sts < 0)
	goto out;

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
    if (out_num)
	*out_num = 0;
    pmfg_reinit_indom(item);

    /* link in */
    item->next = pmfg->items;
    pmfg->items = item;
    return 0;

out:
    free(item);
    return sts;
}

int
pmExtendFetchGroup_event(pmFG pmfg,
		const char *metric, const char *instance,
		const char *field, const char *scale,
		struct timespec out_times[], pmAtomValue out_values[],
		int out_type, int out_stss[],
		unsigned int out_maxnum, unsigned int *out_num, int *out_sts)
{
    int sts;
    pmFGI item;

    if (pmfg == NULL || metric == NULL)
	return -EINVAL;

    sts = pmUseContext(pmfg->ctx);
    if (sts != 0)
	return sts;

    item = calloc(1, sizeof(*item));
    if (item == NULL)
	return -ENOMEM;

    item->type = pmfg_event;

    sts = pmfg_lookup_event(metric, instance, field, item);
    if (sts != 0) {
	/*
	 * If this is an archive, the instance/indom pair may not be
	 * present as of the current moment.  Seek to the end of the
	 * archive temporarily to try again there.
	 */
	__pmContext *ctxp = __pmHandleToPtr(pmfg->ctx);

	if (ctxp->c_type == PM_CONTEXT_ARCHIVE) {
	    struct timeval saved_origin;
	    struct timeval archive_end;
	    int saved_mode, saved_delta;

	    saved_origin.tv_sec = ctxp->c_origin.tv_sec;
	    saved_origin.tv_usec = ctxp->c_origin.tv_usec;
	    saved_mode = ctxp->c_mode;
	    saved_delta = ctxp->c_delta;
	    sts = pmGetArchiveEnd_ctx(ctxp, &archive_end);
	    PM_UNLOCK(ctxp->c_lock);
	    if (sts < 0)
		goto out;
	    sts = pmSetMode(PM_MODE_BACK, &archive_end, 0);
	    if (sts < 0)
		goto out;
	    /* try again */
	    sts = pmfg_lookup_event(metric, instance, field, item);
	    /* go back to saved position */
	    pmSetMode(saved_mode, &saved_origin, saved_delta);
	    if (sts < 0)
		goto out;
	} else { /* not archive */
	    PM_UNLOCK(ctxp->c_lock);
	    goto out;
	}
    }

    if (item->u.event.metric_desc.type != PM_TYPE_EVENT &&
	item->u.event.metric_desc.type != PM_TYPE_HIGHRES_EVENT) {
	sts = PM_ERR_TYPE;
	goto out;
    }

    sts = pmfg_prep_conversion(&item->u.event.field_desc, scale,
				&item->u.event.conv, out_type);
    if (sts != 0)
	goto out;

    /*
     * Reject rate conversion requests, since it's not clear what to
     * refer to; (which field of) previous event record?  which
     * previous field in the same record?
     */
    if (item->u.event.conv.rate_convert) {
	sts = PM_ERR_CONV;
	goto out;
    }

    sts = pmfg_add_pmid(pmfg, item->u.event.metric_pmid);
    if (sts < 0)
	goto out;

    item->u.event.output_values = out_values;
    item->u.event.output_times = out_times;
    item->u.event.output_type = out_type;
    item->u.event.output_stss = out_stss;
    item->u.event.output_sts = out_sts;
    item->u.event.output_maxnum = out_maxnum;
    item->u.event.output_num = out_num;

    /* ensure no stale pointers/content before first reinit */
    if (out_values)
	memset(out_values, 0, sizeof(pmAtomValue) * out_maxnum);
    if (out_num)
	*out_num = 0;
    pmfg_reinit_event(item);

    /* link in */
    item->next = pmfg->items;
    pmfg->items = item;
    return 0;

out:
    free(item);
    return sts;
}

/*
 * Call pmFetch() for the whole group.	Unpack/convert/store results
 * and error codes for all items that requested it.
 */
int
pmFetchGroup(pmFG pmfg)
{
    int sts;
    pmFGI item;
    pmResult *newResult;
    pmResult dummyResult;

    if (pmfg == NULL)
	return -EINVAL;

    /*
     * Walk the fetchgroup, reinitializing every output spot, regardless of
     * later errors.
     */
    for (item = pmfg->items; item; item = item->next) {
	switch (item->type) {
	    case pmfg_timestamp:
		pmfg_reinit_timestamp(item);
		break;
	    case pmfg_item:
		if (item->u.item.metric_desc.sem != PM_SEM_DISCRETE)
		    pmfg_reinit_item(item); /* preserve DISCRETE */
		break;
	    case pmfg_indom:
		if (item->u.indom.metric_desc.sem != PM_SEM_DISCRETE)
		    pmfg_reinit_indom(item); /* preserve DISCRETE */
		break;
	    case pmfg_event:
		/* DISCRETE mode doesn't make sense for an event vector */
		pmfg_reinit_event(item);
		break;
	    default:
		assert(0);	/* can't happen */
	}
    }

    sts = pmUseContext(pmfg->ctx);
    if (sts != 0)
	return sts;

    sts = pmFetch(pmfg->num_unique_pmids, pmfg->unique_pmids, &newResult);
    if (sts < 0 || newResult == NULL) {
	/*
	 * Populate an empty fetch result, which will send out the
	 * appropriate PM_ERR_VALUE etc. indications to the fetchgroup
	 * items.
	 */
	gettimeofday(&dummyResult.timestamp, NULL);
	dummyResult.numpmid = 0;
	dummyResult.vset[0] = NULL;
	newResult = &dummyResult;
    }

    /* Sort instances so that the indom fetchgroups come out conveniently */
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
	    case pmfg_event:
		pmfg_fetch_event(pmfg, item, newResult);
		break;
	    default:
		assert(0);	/* can't happen */
	}
    }

    /*
     * Store new result as previous, if there was one.	If we
     * encountered a pmFetch error this time, retain the previous
     * results, as a rate-conversion reference for a future successful
     * pmFetch.
     */
    if (newResult != &dummyResult) {
	if (pmfg->prevResult)
	    pmFreeResult(pmfg->prevResult);
	pmfg->prevResult = newResult;
    }

    /* NB: we pass through the pmFetch() sts. */
    return sts;
}

/*
 * Clear the fetchgroup of all items, keeping the PMAPI context alive.
 */
int
pmClearFetchGroup(pmFG pmfg)
{
    pmFGI item;

    if (pmfg == NULL)
	return -EINVAL;

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
	    case pmfg_event:
		pmfg_reinit_event(item);
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
    pmfg->items = NULL;

    if (pmfg->prevResult)
	pmFreeResult(pmfg->prevResult);
    pmfg->prevResult = NULL;
    if (pmfg->unique_pmids)
	free(pmfg->unique_pmids);
    pmfg->unique_pmids = NULL;
    pmfg->num_unique_pmids = 0;

    return pmfg_clear_profile(pmfg);
}

/*
 * Destroy the fetchgroup; release all items and related dynamic data.
 */
int
pmDestroyFetchGroup(pmFG pmfg)
{
    int ctx = pmfg->ctx;

    pmfg->ctx = -EINVAL;
    pmDestroyContext(ctx);
    pmClearFetchGroup(pmfg);
    free(pmfg);
    return 0;
}
