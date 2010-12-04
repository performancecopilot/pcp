/*
 * Unpack an array of event records
 * Free space from unpack
 *
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
 */

#include "pmapi.h"
#include "impl.h"

/*
 * Dump a packed array of event records ... need to be paranoid
 * with checking here, because typically called after
 * __pmCheckEventRecords() finds an error
 */
void
__pmDumpEventRecords(FILE *f, pmValueSet *vsp)
{
    pmEventArray	*eap;
    char		*base;
    char		*valend;	/* end of the value */
    pmEventRecord	*erp;
    pmEventParameter	*epp;
    char		*vbuf;
    int			r;	/* records */
    int			p;	/* parameters in a record ... */
    pmAtomValue		atom;

    fprintf(f, "Event Records Dump ...\n");
    fprintf(f, "PMID: %s numval: %d", pmIDStr(vsp->pmid), vsp->numval);
    if (vsp->numval <= 0) {
	fprintf(f, "\nError: bad numval\n");
	return;
    }
    fprintf(f, " valfmt: %d inst: %d", vsp->valfmt, vsp->vlist[0].inst);
    if (vsp->valfmt != PM_VAL_DPTR && vsp->valfmt != PM_VAL_SPTR) {
	fprintf(f, "\nError: bad valfmt\n");
	return;
    }
    eap = (pmEventArray *)vsp->vlist[0].value.pval;
    fprintf(f, " vtype: %s vlen: %d\n", pmTypeStr(eap->ea_type), eap->ea_len);
    if (eap->ea_type != PM_TYPE_EVENT) {
	fprintf(f, "Error: bad vtype\n");
	return;
    }
    if (eap->ea_len < PM_VAL_HDR_SIZE + sizeof(eap->ea_nrecords)) {
	fprintf(f, "Error: bad len (smaller than minimum size %d)\n", PM_VAL_HDR_SIZE + sizeof(eap->ea_nrecords));
	return;
    }
    fprintf(f, "nrecords: %d\n", eap->ea_nrecords);
    if (eap->ea_nrecords < 0) {
	fprintf(f, "Error: bad nrecords\n");
	return;
    }
    if (eap->ea_nrecords == 0) {
	fprintf(f, "Warning: no event records\n");
	return;
    }

    /* have something plausible to report in the array buffer ... */
    base = (char *)&eap->ea_record[0];
    valend = &((char *)eap)[eap->ea_len];
    for (r = 0; r < eap->ea_nrecords; r++) {
	fprintf(f, "Event Record [%d]", r);
	if (base + sizeof(erp->er_timestamp) + sizeof(erp->er_flags) + sizeof(erp->er_nparams) > valend) {
	    fprintf(f, " Error: buffer overflow\n");
	    return;
	}
	erp = (pmEventRecord *)base;
	if (erp->er_flags != 0)
	    fprintf(f, " flags=%x", erp->er_flags);
	base += sizeof(erp->er_timestamp) + sizeof(erp->er_flags) + sizeof(erp->er_nparams);
	if (erp->er_flags == PM_ER_FLAG_MISSED) {
	    fprintf(f, "\n    ==> %d missed records\n", erp->er_nparams);
	    continue;
	}
	fprintf(f, " with %d parameters\n", erp->er_nparams);
	for (p = 0; p < erp->er_nparams; p++) {
	    char	*name;
	    fprintf(f, "    Parameter [%d]:", p);
	    if (base + sizeof(pmEventParameter) > valend) {
		fprintf(f, " Error: buffer overflow\n");
		return;
	    }
	    epp = (pmEventParameter *)base;
	    if (base + sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len) > valend) {
		fprintf(f, " Error: buffer overflow\n");
		return;
	    }
	    if (pmNameID(epp->ep_pmid, &name) == 0) {
		fprintf(f, " %s", name);
		free(name);
	    }
	    else
		fprintf(f, " %s", pmIDStr(epp->ep_pmid));
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
		    fprintf(f, " = %lli", atom.ll);
		    break;
		case PM_TYPE_U64:
		    memcpy((void *)&atom.ull, (void *)vbuf, sizeof(atom.ull));
		    fprintf(f, " = %llu", atom.ull);
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
		    fprintf(f, " = \"%*.*s\"", epp->ep_len-PM_VAL_HDR_SIZE, epp->ep_len-PM_VAL_HDR_SIZE, vbuf);
		    break;
		case PM_TYPE_AGGREGATE:
		case PM_TYPE_AGGREGATE_STATIC:
		    fprintf(f, " = [%08x...]", ((__uint32_t *)vbuf)[0]);
		    break;
		default:
		    fprintf(f, " : bad type %s", pmTypeStr(epp->ep_type));
	    }
	    fputc('\n', f);
	    base += sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len);
	}
    }
    return;
}

/*
 * Integrity checker for a packed array of event records
 */
int
__pmCheckEventRecords(pmValueSet *vsp)
{
    pmEventArray	*eap;
    char		*base;
    char		*valend;	/* end of the value */
    pmEventRecord	*erp;
    pmEventParameter	*epp;
    int			r;	/* records */
    int			p;	/* parameters in a record ... */
    int			nparams;

    if (vsp->numval < 1)
	return vsp->numval;
    if (vsp->numval > 1)
	return PM_ERR_TOOBIG;
    if (vsp->valfmt != PM_VAL_DPTR && vsp->valfmt != PM_VAL_SPTR)
	return PM_ERR_CONV;
    eap = (pmEventArray *)vsp->vlist[0].value.pval;
    if (eap->ea_type != PM_TYPE_EVENT)
	return PM_ERR_TYPE;
    if (eap->ea_len < PM_VAL_HDR_SIZE + sizeof(eap->ea_nrecords))
	return PM_ERR_TOOSMALL;
    if (eap->ea_nrecords < 0)
	return PM_ERR_TOOSMALL;
    base = (char *)&eap->ea_record[0];
    valend = &((char *)eap)[eap->ea_len];
    /* header seems OK, onto each event record */
    for (r = 0; r < eap->ea_nrecords; r++) {
	if (base + sizeof(erp->er_timestamp) + sizeof(erp->er_flags) + sizeof(erp->er_nparams) > valend)
	    return PM_ERR_TOOBIG;
	erp = (pmEventRecord *)base;
	base += sizeof(erp->er_timestamp) + sizeof(erp->er_flags) + sizeof(erp->er_nparams);
	if (erp->er_flags == PM_ER_FLAG_MISSED)
	    nparams = 0;
	else
	    nparams = erp->er_nparams;
	for (p = 0; p < nparams; p++) {
	    if (base + sizeof(pmEventParameter) > valend)
		return PM_ERR_TOOBIG;
	    epp = (pmEventParameter *)base;
	    if (base + sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len) > valend)
		return PM_ERR_TOOBIG;
	    base += sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len);
	}
    }
    return 0;
}

int
pmUnpackEventRecords(pmValueSet *vsp, pmResult ***rap)
{
    pmEventArray	*eap;
    pmEventRecord	*erp;
    pmEventParameter	*epp;
    pmResult		*rp;
    char		*base;
    char		*vbuf;
    char		*valend;	/* end of the value */
    int			r;		/* records */
    int			p;		/* parameters in a record ... */
    int			numpmid;	/* metrics in a pmResult */
    int			need;
    int			vsize;
    int			sts;
    static int		first = 1;
    static char		*name_flags = "event.flags";
    static char		*name_missed = "event.missed";

    if (first) {
	sts = __pmRegisterAnon(name_flags, PM_TYPE_U32);
	if (sts < 0) {
	    fprintf(stderr, "pmUnpackEventRecords: Warning: failed to register %s: %s\n", name_flags, pmErrStr(sts));
	    return sts;
	}
	sts = __pmRegisterAnon(name_missed, PM_TYPE_U32);
	if (sts < 0) {
	    fprintf(stderr, "pmUnpackEventRecords: Warning: failed to register %s: %s\n", name_missed, pmErrStr(sts));
	    return sts;
	}
	first = 0;
    }

    sts = __pmCheckEventRecords(vsp);
    if (sts < 0) {
	__pmDumpEventRecords(stderr, vsp);
	return sts;
    }

    eap = (pmEventArray *)vsp->vlist[0].value.pval;
    if (eap->ea_nrecords == 0) {
	*rap = NULL;
	return 0;
    }

    /*
     * allocate one more than needed as a NULL sentinal to be used
     * in pmFreeEventResult
     */
    *rap = (pmResult **)malloc((eap->ea_nrecords+1) * sizeof(pmResult *));
    if (*rap == NULL) {
	return -errno;
    }

    base = (char *)&eap->ea_record[0];
    valend = &((char *)eap)[eap->ea_len];
    /* walk packed event record array */
    for (r = 0; r < eap->ea_nrecords; r++) {
	rp = NULL;
	erp = (pmEventRecord *)base;
	/*
	 * er_flags optionally unpacked into an extra anon events.flags metric
	 * before all the event record parameters, and for PM_ER_FLAG_MISSED
	 * er_nparams is a count of the missed records.
	 */
	if (erp->er_flags == 0)
	    numpmid = erp->er_nparams;
	else if (erp->er_flags == PM_ER_FLAG_MISSED)
	    numpmid = 2;
	else
	    numpmid = erp->er_nparams + 1;
	need = sizeof(pmResult) + (numpmid-1)*sizeof(pmValueSet *);
	rp = (pmResult *)malloc(need); 
	if (rp == NULL) {
	    sts = -errno;
	    r--;
	    goto bail;
	}
	(*rap)[r] = rp;
	rp->timestamp.tv_sec = erp->er_timestamp.tv_sec;
	rp->timestamp.tv_usec = erp->er_timestamp.tv_usec;
	rp->numpmid = numpmid;
	base += sizeof(erp->er_timestamp) + sizeof(erp->er_flags) + sizeof(erp->er_nparams);
	for (p = 0; p < numpmid; p++) {
	    /* always have numval == 1 */
	    rp->vset[p] = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
	    if (rp->vset[p] == NULL) {
		rp->numpmid = p;
		sts = -errno;
		goto bail;
	    }
	    if (p == 0 && erp->er_flags != 0) {
		/* rewrite non-zero er_flags as the anon event.flags metric */
		static pmID	pmid_flags = 0;
		int		lsts;
		if (pmid_flags == 0) {
		    lsts = pmLookupName(1, &name_flags, &pmid_flags);
		    if (lsts < 0) {
			fprintf(stderr, "pmUnpackEventRecords: Warning: failed to get PMID for %s: %s\n", name_flags, pmErrStr(lsts));
			__pmid_int(&pmid_flags)->item = 1;
		    }
		}
		rp->vset[p]->pmid = pmid_flags;
		rp->vset[p]->numval = 1;
		rp->vset[p]->vlist[0].inst = PM_IN_NULL;
		rp->vset[p]->valfmt = PM_VAL_INSITU;
		rp->vset[p]->vlist[0].value.lval = erp->er_flags;
		continue;
	    }
	    if (p == 1 && erp->er_flags == PM_ER_FLAG_MISSED) {
		/* rewrite missed count as the anon event.missed metric */
		static pmID	pmid_missed = 0;
		int		lsts;
		if (pmid_missed == 0) {
		    lsts = pmLookupName(1, &name_missed, &pmid_missed);
		    if (lsts < 0) {
			fprintf(stderr, "pmUnpackEventRecords: Warning: failed to get PMID for %s: %s\n", name_missed, pmErrStr(lsts));
			__pmid_int(&pmid_missed)->item = 1;
		    }
		}
		rp->vset[p]->pmid = pmid_missed;
		rp->vset[p]->numval = 1;
		rp->vset[p]->vlist[0].inst = PM_IN_NULL;
		rp->vset[p]->valfmt = PM_VAL_INSITU;
		rp->vset[p]->vlist[0].value.lval = erp->er_nparams;
		continue;
	    }
	    epp = (pmEventParameter *)base;
	    rp->vset[p]->pmid = epp->ep_pmid;
	    rp->vset[p]->numval = 1;
	    rp->vset[p]->vlist[0].inst = PM_IN_NULL;
	    vbuf = (char *)epp + sizeof(epp->ep_pmid) + sizeof(int);
	    switch (epp->ep_type) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    rp->vset[p]->valfmt = PM_VAL_INSITU;
		    memcpy((void *)&rp->vset[p]->vlist[0].value.lval, (void *)vbuf, sizeof(__int32_t));
		    goto done;
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
		default:
		    while (p-- >= 0)
			__pmPoolFree(rp->vset[p], sizeof(pmValueSet));
		    while (r-- >= 0)
			pmFreeResult((*rap)[r]);
		    return PM_ERR_TYPE;
	    }
	    need = vsize + PM_VAL_HDR_SIZE;
	    if (vsize == sizeof(__int64_t)) {
		rp->vset[p]->vlist[0].value.pval = (pmValueBlock *)__pmPoolAlloc(need);
	    }
	    else {
		int	want = need;
		if (want < sizeof(pmValueBlock))
		    want = sizeof(pmValueBlock);
		rp->vset[p]->vlist[0].value.pval = (pmValueBlock *)malloc(want);
	    }
	    if (rp->vset[p]->vlist[0].value.pval == NULL) {
		sts = -errno;
		rp->vset[p]->valfmt = PM_VAL_INSITU;
		goto bail;
	    }
	    rp->vset[p]->vlist[0].value.pval->vlen = need;
	    rp->vset[p]->vlist[0].value.pval->vtype = epp->ep_type;
	    memcpy((void *)rp->vset[p]->vlist[0].value.pval->vbuf, (void *)vbuf, vsize);
	    rp->vset[p]->valfmt = PM_VAL_DPTR;
	    
done:
	    base += sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len);
	}
    }
    (*rap)[r] = NULL;		/* sentinal */

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
