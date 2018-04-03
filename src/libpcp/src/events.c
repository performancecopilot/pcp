/*
 * Unpack an array of event records
 * Free space from unpack
 *
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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
 * Thread-safe notes
 *
 * The initialization of pmid_flags and pmid_missed both have a potential
 * race, but there are no side-effects and the end result will be the
 * same, so no locking is required.
 *
 */
#include <inttypes.h>
#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#include "fault.h"

static void
dump_count(FILE *f, size_t length, int nrecords)
{
    if (length < PM_VAL_HDR_SIZE + sizeof(int)) {
	fprintf(f, "Error: bad len (smaller than minimum size %d)\n",
		PM_VAL_HDR_SIZE + (int)sizeof(int));
	return;
    }
    fprintf(f, "nrecords: %d\n", nrecords);
    if (nrecords < 0) {
	fprintf(f, "Error: bad nrecords\n");
	return;
    }
    if (nrecords == 0) {
	fprintf(f, "Warning: no event records\n");
	return;
    }
}

static int
dump_flags(FILE *f, unsigned int flags, int nparams)
{
    if (flags != 0)
	fprintf(f, " flags=%x", flags);
    if (flags & PM_EVENT_FLAG_MISSED) {
	fprintf(f, "\n    ==> %d missed records", nparams);
	if (flags != PM_EVENT_FLAG_MISSED)
	    fprintf(f, " (Warning: extra flags %x ignored)",
		    flags & (~PM_EVENT_FLAG_MISSED));
	fputc('\n', f);
	return 1;
    }
    return 0;
}

static void
dump_parameter(FILE *f, pmEventParameter *epp)
{
    pmAtomValue	atom;
    char	strbuf[20];
    char	*vbuf;
    int		numnames;
    char	**names;

    if ((numnames = pmNameAll(epp->ep_pmid, &names)) > 0) {
	fprintf(f, " ");
	__pmPrintMetricNames(f, numnames, names, " or ");
	free(names);
    } else {
	fprintf(f, " %s", pmIDStr_r(epp->ep_pmid, strbuf, sizeof(strbuf)));
    }

    vbuf = (char *)epp + sizeof(epp->ep_pmid) + sizeof(int);
    switch (epp->ep_type) {
	case PM_TYPE_32:
	    fprintf(f, " = %i", *((__int32_t *)vbuf));
	    break;
	case PM_TYPE_U32:
	    fprintf(f, " = %u", *((__uint32_t *)vbuf));
	    break;
	case PM_TYPE_64:
	    memcpy((void *)&atom.ll, (void *)vbuf, sizeof(atom.ll));
	    fprintf(f, " = %"PRIi64, atom.ll);
	    break;
	case PM_TYPE_U64:
	    memcpy((void *)&atom.ull, (void *)vbuf, sizeof(atom.ull));
	    fprintf(f, " = %"PRIu64, atom.ull);
	    break;
	case PM_TYPE_FLOAT:
	    memcpy((void *)&atom.f, (void *)vbuf, sizeof(atom.f));
	    fprintf(f, " = %.8g", (double)atom.f);
	    break;
	case PM_TYPE_DOUBLE:
	    memcpy((void *)&atom.d, (void *)vbuf, sizeof(atom.d));
	    fprintf(f, " = %.16g", atom.d);
	    break;
	case PM_TYPE_STRING:
	    fprintf(f, " = \"%*.*s\"", epp->ep_len-PM_VAL_HDR_SIZE,
		    epp->ep_len-PM_VAL_HDR_SIZE, vbuf);
	    break;
	case PM_TYPE_AGGREGATE:
	case PM_TYPE_AGGREGATE_STATIC:
	    fprintf(f, " = [%08x...]", ((__uint32_t *)vbuf)[0]);
	    break;
	case PM_TYPE_UNKNOWN:
	    fprintf(f, " : unknown type");
	    break;
	default:
	    fprintf(f, " : bad type %u", epp->ep_type);
	    break;
    }
    fputc('\n', f);
}

/*
 * Dump a packed array of event records ... need to be paranoid
 * with checking here, because typically called after
 * __pmCheck[HighRes]EventRecords() finds an error.
 * Process the idx'th instance.
 */
void
dump_event_records(FILE *f, pmValueSet *vsp, int idx, int highres)
{
    char	*base;
    char	*valend;	/* end of the value */
    char	strbuf[20];
    size_t	length;
    int		nrecords;
    int		nparams;
    int		r;	/* records index */
    int		p;	/* parameters in a record ... */

    fprintf(f, "Event Records Dump ...\n");
    fprintf(f, "PMID: %s numval: %d",
		pmIDStr_r(vsp->pmid, strbuf, sizeof(strbuf)), vsp->numval);
    if (vsp->numval <= 0) {
	fprintf(f, "\nError: bad numval\n");
	return;
    }
    fprintf(f, " valfmt: %d", vsp->valfmt);
    if (vsp->valfmt != PM_VAL_DPTR && vsp->valfmt != PM_VAL_SPTR) {
	fprintf(f, "\nError: bad valfmt\n");
	return;
    }
    if (vsp->vlist[idx].inst != PM_IN_NULL)
	fprintf(f, " inst: %d", vsp->vlist[idx].inst);

    if (highres) {
	pmHighResEventArray	*hreap;

	hreap = (pmHighResEventArray *)vsp->vlist[idx].value.pval;
	fprintf(f, " vtype: %s vlen: %d\n",
		pmTypeStr_r(hreap->ea_type, strbuf, sizeof(strbuf)),
		hreap->ea_len);
	if (hreap->ea_type != PM_TYPE_HIGHRES_EVENT) {
	    fprintf(f, "Error: bad highres vtype\n");
	    return;
	}
	length = hreap->ea_len;
	nrecords = hreap->ea_nrecords;
	valend = &((char *)hreap)[length];
	base = (char *)&hreap->ea_record[0];
    }
    else {
	pmEventArray	*eap;

	eap = (pmEventArray *)vsp->vlist[idx].value.pval;
	fprintf(f, " vtype: %s vlen: %d\n",
		pmTypeStr_r(eap->ea_type, strbuf, sizeof(strbuf)), eap->ea_len);
	if (eap->ea_type != PM_TYPE_EVENT) {
	    fprintf(f, "Error: bad vtype\n");
	    return;
	}
	length = eap->ea_len;
	nrecords = eap->ea_nrecords;
	valend = &((char *)eap)[length];
	base = (char *)&eap->ea_record[0];
    }
    dump_count(f, length, nrecords);

    for (r = 0; r < nrecords; r++) {
	pmEventParameter *epp;
	unsigned int flags;
	size_t size;

	fprintf(f, "Event Record [%d]", r);

	if (highres) {
	    pmHighResEventRecord *herp = (pmHighResEventRecord *)base;

	    size = sizeof(herp->er_timestamp) + sizeof(herp->er_flags) +
		    sizeof(herp->er_nparams);
	    if (base + size > valend) {
		fprintf(f, " Error: buffer overflow\n");
		return;
	    }
	    flags = herp->er_flags;
	    nparams = herp->er_nparams;
	} else {
	    pmEventRecord	*erp = (pmEventRecord *)base;

	    size = sizeof(erp->er_timestamp) + sizeof(erp->er_flags) +
		    sizeof(erp->er_nparams);
	    if (base + size > valend) {
		fprintf(f, " Error: buffer overflow\n");
		return;
	    }
	    flags = erp->er_flags;
	    nparams = erp->er_nparams;
	}
	base += size;

	if (dump_flags(f, flags, nparams) != 0)
	    continue;

	fprintf(f, " with %d parameters\n", nparams);
	for (p = 0; p < nparams; p++) {
	    fprintf(f, "    Parameter [%d]:", p);
	    if (base + sizeof(pmEventParameter) > valend) {
		fprintf(f, " Error: buffer overflow\n");
		return;
	    }
	    epp = (pmEventParameter *)base;
	    size = sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len);
	    if (base + size > valend) {
		fprintf(f, " Error: buffer overflow\n");
		return;
	    }
	    dump_parameter(f, epp);
	    base += size;
	}
    }
}

void
__pmDumpEventRecords(FILE *f, pmValueSet *vsp, int idx)
{
    dump_event_records(f, vsp, idx, 0);
}

void
__pmDumpHighResEventRecords(FILE *f, pmValueSet *vsp, int idx)
{
    dump_event_records(f, vsp, idx, 1);
}

/*
 * Integrity checker for a packed array of event records, check
 * the idx'th instance.
 */
int
check_event_records(pmValueSet *vsp, int idx, int highres)
{
    char		*base;
    char		*valend;	/* end of the value */
    pmEventParameter	*epp;
    int			nrecords;
    int			nparams;
    int			r;	/* records */
    int			p;	/* parameters in a record ... */

    if (vsp->numval < 1)
	return vsp->numval;
    if (vsp->valfmt != PM_VAL_DPTR && vsp->valfmt != PM_VAL_SPTR)
	return PM_ERR_CONV;

    if (highres) {
	pmHighResEventArray	*hreap;

	hreap = (pmHighResEventArray *)vsp->vlist[idx].value.pval;
	if (hreap->ea_type != PM_TYPE_HIGHRES_EVENT)
	    return PM_ERR_TYPE;
	if (hreap->ea_len < PM_VAL_HDR_SIZE + sizeof(int))
	    return PM_ERR_TOOSMALL;
	nrecords = hreap->ea_nrecords;
	base = (char *)&hreap->ea_record[0];
	valend = &((char *)hreap)[hreap->ea_len];
    }
    else {
	pmEventArray	*eap;

	eap = (pmEventArray *)vsp->vlist[idx].value.pval;
	if (eap->ea_type != PM_TYPE_EVENT)
	    return PM_ERR_TYPE;
	if (eap->ea_len < PM_VAL_HDR_SIZE + sizeof(eap->ea_nrecords))
	    return PM_ERR_TOOSMALL;
	nrecords = eap->ea_nrecords;
	base = (char *)&eap->ea_record[0];
	valend = &((char *)eap)[eap->ea_len];
    }
    if (nrecords < 0)
	return PM_ERR_TOOSMALL;

    /* header seems OK, onto each event record */
    for (r = 0; r < nrecords; r++) {
	unsigned int flags;
	size_t size;

	if (highres) {
	    pmHighResEventRecord *hrerp = (pmHighResEventRecord *)base;

	    size = sizeof(hrerp->er_timestamp) + sizeof(hrerp->er_flags) +
		   sizeof(hrerp->er_nparams);
	    if (base + size > valend)
		return PM_ERR_TOOBIG;
	    flags = hrerp->er_flags;
	    nparams = hrerp->er_nparams;
	}
	else {
	    pmEventRecord	*erp = (pmEventRecord *)base;

	    size = sizeof(erp->er_timestamp) + sizeof(erp->er_flags) +
		   sizeof(erp->er_nparams);
	    if (base + size > valend)
		return PM_ERR_TOOBIG;
	    flags = erp->er_flags;
	    nparams = erp->er_nparams;
	}
	base += size;

	if (flags & PM_EVENT_FLAG_MISSED) {
	    if (flags == PM_EVENT_FLAG_MISSED)
		nparams = 0;
	    else {
		/*
		 * not legal to have other flag bits set when
		 * PM_EVENT_FLAG_MISSED is set
		 */
		return PM_ERR_CONV;
	    }
	}

	for (p = 0; p < nparams; p++) {
	    if (base + sizeof(pmEventParameter) > valend)
		return PM_ERR_TOOBIG;
	    epp = (pmEventParameter *)base;
	    size = sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len);
	    if (base + size > valend)
		return PM_ERR_TOOBIG;
	    base += size;
	}
    }
    return 0;
}

int
__pmCheckEventRecords(pmValueSet *vsp, int idx)
{
    return check_event_records(vsp, idx, 0);
}

static int
__pmCheckHighResEventRecords(pmValueSet *vsp, int idx)
{
    return check_event_records(vsp, idx, 1);
}

/*
 * flags is optionally unpacked into an extra anon events.flags metric
 * before all the event record parameters, and for PM_EVENT_FLAG_MISSED
 * nparams is a count of the missed records.
 */
static int
count_event_parameters(unsigned int flags, int nparams)
{
    if (flags == 0)
	return nparams;
    else if (flags & PM_EVENT_FLAG_MISSED)
	return 2;
    return nparams + 1;
}

static int
add_event_parameter(const char *caller, pmEventParameter *epp, int idx,
		    unsigned int flags, int nparams, pmValueSet **vsetp)
{
    pmValueSet		*vset;
    char		*vbuf;
    char		errmsg[PM_MAXERRMSGLEN];
    int			sts;
    int			need;
    int			want;
    int			vsize;

    /* always have numval == 1 */
PM_FAULT_POINT("libpcp/" __FILE__ ":2", PM_FAULT_ALLOC);
    if ((vset = (pmValueSet *)malloc(sizeof(pmValueSet))) == NULL)
	return -oserror();

    if (idx == 0 && flags != 0) {
	/* rewrite non-zero er_flags as the anon event.flags metric */
	static pmID	pmid_flags = 0;

	if (pmid_flags == 0) {
	    char	*name = "event.flags";
	    if ((sts = pmLookupName(1, &name, &pmid_flags)) < 0) {
		fprintf(stderr, "%s: Warning: failed to get PMID for %s: %s\n",
			caller, name, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		pmid_flags = pmID_build(pmID_domain(pmid_flags), pmID_cluster(pmid_flags), 1);
	    }
	}
	vset->pmid = pmid_flags;
	vset->numval = 1;
	vset->vlist[0].inst = PM_IN_NULL;
	vset->valfmt = PM_VAL_INSITU;
	vset->vlist[0].value.lval = flags;
	*vsetp = vset;
	return 1;
    }
    if (idx == 1 && flags & PM_EVENT_FLAG_MISSED) {
	/* rewrite missed count as the anon event.missed metric */
	static pmID	pmid_missed = 0;

	if (pmid_missed == 0) {
	    char	*name = "event.missed";
	    if ((sts = pmLookupName(1, &name, &pmid_missed)) < 0) {
		fprintf(stderr, "%s: Warning: failed to get PMID for %s: %s\n",
			caller, name, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		pmid_missed = pmID_build(pmID_domain(pmid_missed), pmID_cluster(pmid_missed), 1);
	    }
	}
	vset->pmid = pmid_missed;
	vset->numval = 1;
	vset->vlist[0].inst = PM_IN_NULL;
	vset->valfmt = PM_VAL_INSITU;
	vset->vlist[0].value.lval = nparams;
	*vsetp = vset;
	return 2;
    }

    vset->pmid = epp->ep_pmid;
    vset->numval = 1;
    vset->vlist[0].inst = PM_IN_NULL;
    vbuf = (char *)epp + sizeof(epp->ep_pmid) + sizeof(int);
    switch (epp->ep_type) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	    vset->valfmt = PM_VAL_INSITU;
	    memcpy((void *)&vset->vlist[0].value.lval, (void *)vbuf, sizeof(__int32_t));
	    *vsetp = vset;
	    return 0;

	case PM_TYPE_64:
	case PM_TYPE_U64:
	    vsize = sizeof(__int64_t);
	    break;
	case PM_TYPE_FLOAT:
	    vsize = sizeof(float);
	    break;
	case PM_TYPE_DOUBLE:
	    vsize = sizeof(double);
	    break;
	case PM_TYPE_AGGREGATE:
	case PM_TYPE_STRING:
	case PM_TYPE_AGGREGATE_STATIC:
	    vsize = epp->ep_len - PM_VAL_HDR_SIZE;
	    break;
	case PM_TYPE_EVENT:	/* no nesting! */
	case PM_TYPE_HIGHRES_EVENT:
	default:
	    free(vset);
	    return PM_ERR_TYPE;
    }
    need = vsize + PM_VAL_HDR_SIZE;
    want = need;
    if (want < sizeof(pmValueBlock))
	want = sizeof(pmValueBlock);
PM_FAULT_POINT("libpcp/" __FILE__ ":3", PM_FAULT_ALLOC);
    vset->vlist[0].value.pval = (pmValueBlock *)malloc(want);
    if (vset->vlist[0].value.pval == NULL) {
	vset->valfmt = PM_VAL_INSITU;
	return -oserror();
    }
    vset->vlist[0].value.pval->vlen = need;
    vset->vlist[0].value.pval->vtype = epp->ep_type;
    memcpy((void *)vset->vlist[0].value.pval->vbuf, (void *)vbuf, vsize);
    vset->valfmt = PM_VAL_DPTR;
    *vsetp = vset;
    return 0;
}

/*
 * Process the idx'th instance of an event record metric value
 * and unpack the array of event records into a pmResult.
 *
 * Internal variant of pmUnpackEventRecords() with current context ptr.
 */
static int
UnpackEventRecords(__pmContext *ctxp, pmValueSet *vsp, int idx, pmResult ***rap)
{
    pmEventArray	*eap;
    const char		caller[] = "pmUnpackEventRecords";
    char		*base;
    size_t		need;
    int			r;		/* records */
    int			p;		/* parameters in a record ... */
    int			numpmid;	/* metrics in a pmResult */
    int			sts;

    if ((sts = __pmCheckEventRecords(vsp, idx)) < 0) {
	__pmDumpEventRecords(stderr, vsp, idx);
	return sts;
    }

    eap = (pmEventArray *)vsp->vlist[idx].value.pval;
    if (eap->ea_nrecords == 0) {
	*rap = NULL;
	return 0;
    }

    /*
     * allocate one more than needed as a NULL sentinel to be used
     * in pmFreeEventResult
     */
PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_ALLOC);
    need = (eap->ea_nrecords + 1) * sizeof(pmResult *);
    if ((*rap = (pmResult **)malloc(need)) == NULL)
	return -oserror();

    base = (char *)&eap->ea_record[0];
    /* walk packed event record array */
    for (r = 0; r < eap->ea_nrecords; r++) {
	pmEventRecord	*erp = (pmEventRecord *)base;
	pmResult	*rp;

	numpmid = count_event_parameters(erp->er_flags, erp->er_nparams);
	need = sizeof(pmResult) + (numpmid-1)*sizeof(pmValueSet *);
PM_FAULT_POINT("libpcp/" __FILE__ ":4", PM_FAULT_ALLOC);
	if ((rp = (pmResult *)malloc(need)) == NULL) {
	    sts = -oserror();
	    r--;
	    goto bail;
	}
	(*rap)[r] = rp;
	rp->timestamp.tv_sec = erp->er_timestamp.tv_sec;
	rp->timestamp.tv_usec = erp->er_timestamp.tv_usec;
	rp->numpmid = numpmid;
	base += sizeof(erp->er_timestamp) + sizeof(erp->er_flags) + sizeof(erp->er_nparams);
	for (p = 0; p < numpmid; p++) {
	    pmEventParameter *epp = (pmEventParameter *)base;

	    if ((sts = add_event_parameter(caller, epp, p,
				    erp->er_flags, erp->er_nparams,
				    &rp->vset[p])) < 0) {
		rp->numpmid = p;
		goto bail;
	    }
	    if (sts == 0)
		base += sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len);
	}
    }
    (*rap)[r] = NULL;		/* sentinel */

    if (pmDebugOptions.fetch) {
	fprintf(stderr, "pmUnpackEventRecords returns ...\n");
	for (r = 0; r < eap->ea_nrecords; r++) {
	    fprintf(stderr, "pmResult[%d]\n", r);
	    __pmDumpResult_ctx(ctxp, stderr, (*rap)[r]);
	}
    }

    return eap->ea_nrecords;

bail:
    while (r >= 0) {
	if ((*rap)[r] != NULL)
	    pmFreeResult((*rap)[r]);
	r--;
    }
    free(*rap);
    *rap = NULL;
    return sts;
}

int
pmUnpackEventRecords(pmValueSet *vsp, int idx, pmResult ***rap)
{
    int	sts;
    sts = UnpackEventRecords(NULL, vsp, idx, rap);
    return sts;
}

/*
 * Process the idx'th instance of a highres event record metric value
 * and unpack the array of event records into a pmHighResResult.
 */
int
pmUnpackHighResEventRecords(pmValueSet *vsp, int idx, pmHighResResult ***rap)
{
    pmHighResEventArray	*hreap;
    const char		caller[] = "pmUnpackHighResEventRecords";
    char		*base;
    size_t		need;
    int			r;		/* records */
    int			p;		/* parameters in a record ... */
    int			numpmid;	/* metrics in a pmResult */
    int			sts;

    if ((sts = __pmCheckHighResEventRecords(vsp, idx)) < 0) {
	__pmDumpHighResEventRecords(stderr, vsp, idx);
	return sts;
    }

    hreap = (pmHighResEventArray *)vsp->vlist[idx].value.pval;
    if (hreap->ea_nrecords == 0) {
	*rap = NULL;
	return 0;
    }

    /*
     * allocate one more than needed as a NULL sentinel to be used
     * in pmFreeHighResEventResult
     */
PM_FAULT_POINT("libpcp/" __FILE__ ":7", PM_FAULT_ALLOC);
    need = (hreap->ea_nrecords + 1) * sizeof(pmHighResResult *);
    if ((*rap = (pmHighResResult **)malloc(need)) == NULL)
	return -oserror();

    base = (char *)&hreap->ea_record[0];
    /* walk packed event record array */
    for (r = 0; r < hreap->ea_nrecords; r++) {
	pmHighResEventRecord *erp = (pmHighResEventRecord *)base;
	pmHighResResult *rp;

	numpmid = count_event_parameters(erp->er_flags, erp->er_nparams);
	need = sizeof(pmHighResResult) + (numpmid-1)*sizeof(pmValueSet *);
PM_FAULT_POINT("libpcp/" __FILE__ ":8", PM_FAULT_ALLOC);
	if ((rp = (pmHighResResult *)malloc(need)) == NULL) {
	    sts = -oserror();
	    r--;
	    goto bail;
	}
	(*rap)[r] = rp;
	rp->timestamp.tv_sec = erp->er_timestamp.tv_sec;
	rp->timestamp.tv_nsec = erp->er_timestamp.tv_nsec;
	rp->numpmid = numpmid;
	base += sizeof(erp->er_timestamp) + sizeof(erp->er_flags) + sizeof(erp->er_nparams);
	for (p = 0; p < numpmid; p++) {
	    pmEventParameter *epp = (pmEventParameter *)base;

	    if ((sts = add_event_parameter(caller, epp, p,
				    erp->er_flags, erp->er_nparams,
				    &rp->vset[p])) < 0) {
		rp->numpmid = p;
		goto bail;
	    }
	    if (sts == 0)
		base += sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len);
	}
    }
    (*rap)[r] = NULL;		/* sentinel */

    if (pmDebugOptions.fetch) {
	fprintf(stderr, "%s returns ...\n", caller);
	for (r = 0; r < hreap->ea_nrecords; r++) {
	    fprintf(stderr, "pmHighResResult[%d]\n", r);
	    __pmDumpHighResResult(stderr, (*rap)[r]);
	}
    }

    return hreap->ea_nrecords;

bail:
    while (r >= 0) {
	if ((*rap)[r] != NULL)
	    __pmFreeHighResResult((*rap)[r]);
	r--;
    }
    free(*rap);
    *rap = NULL;
    return sts;
}

void
pmFreeEventResult(pmResult **rset)
{
    int		r;

    if (rset == NULL)
	return;
    for (r = 0; rset[r] != NULL; r++)
	pmFreeResult(rset[r]);
    free(rset);
}

void
pmFreeHighResEventResult(pmHighResResult **rset)
{
    int		r;

    if (rset == NULL)
	return;
    for (r = 0; rset[r] != NULL; r++)
	__pmFreeHighResResult(rset[r]);
    free(rset);
}
