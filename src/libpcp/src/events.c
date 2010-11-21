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

int
pmUnpackEventRecords(pmValueBlock *vbp, pmResult ***rap, int *nmissed)
{
    pmEventArray	*eap = (pmEventArray *)vbp;
    pmEventRecord	*erp;
    pmEventParameter	*epp;
    pmResult		*rp;
    char		*base;
    char		*vbuf;
    char		*valend;	/* end of the value */
    int			r;	/* records */
    int			p;	/* parameters in a record ... */
    int			need;
    int			vsize;
    int			sts;

    if (vbp->vtype != PM_TYPE_EVENT)
	return PM_ERR_TYPE;
    if (vbp->vlen < PM_VAL_HDR_SIZE + sizeof(eap->ea_nrecords) + sizeof(eap->ea_nmissed))
	return PM_ERR_TOOSMALL;
    if (eap->ea_nrecords < 0 || eap->ea_nmissed < 0)
	return PM_ERR_TOOSMALL;
    if (nmissed != NULL)
	*nmissed = eap->ea_nmissed;
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
    valend = &((char *)eap)[vbp->vlen];
    /* walk packed event record array */
    for (r = 0; r < eap->ea_nrecords; r++) {
	rp = NULL;
	if (base + sizeof(erp->er_timestamp) + sizeof(erp->er_nparams) > valend) {
	    sts = PM_ERR_TOOBIG;
	    r--;
	    goto bail;
	}
	erp = (pmEventRecord *)base;
	need = sizeof(pmResult) + (erp->er_nparams-1)*sizeof(pmValueSet *);
	rp = (pmResult *)malloc(need); 
	if (rp == NULL) {
	    sts = -errno;
	    r--;
	    goto bail;
	}
	(*rap)[r] = rp;
	rp->timestamp.tv_sec = erp->er_timestamp.tv_sec;
	rp->timestamp.tv_usec = erp->er_timestamp.tv_usec;
	rp->numpmid = erp->er_nparams;
	base += sizeof(erp->er_timestamp) + sizeof(erp->er_nparams);
	for (p = 0; p < erp->er_nparams; p++) {
	    /* always have numval == 1 */
	    rp->vset[p] = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
	    if (rp->vset[p] == NULL) {
		rp->numpmid = p;
		sts = -errno;
		goto bail;
	    }
	    if (base + sizeof(pmEventParameter) > valend) {
		rp->numpmid = p+1;
		sts = PM_ERR_TOOBIG;
		goto bail;
	    }
	    epp = (pmEventParameter *)base;
	    if (base + sizeof(epp->ep_pmid) + PM_PDU_SIZE_BYTES(epp->ep_len) > valend) {
		rp->numpmid = p+1;
		sts = PM_ERR_TOOBIG;
		goto bail;
	    }
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
