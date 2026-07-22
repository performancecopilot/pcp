/*
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2022 Red Hat.
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
#include "libpcp.h"
#include "import.h"
#include "private.h"

int
_pmi_stuff_value(pmi_context *current, pmi_handle *hp, const char *value)
{
    __pmResult	*rp;
    int		i;
    pmID	pmid;
    pmValueSet	*vsp;
    pmValue	*vp;
    pmi_metric	*mp;
    char	*end;
    int		dsize;
    void	*data;
    __int64_t	ll;
    __uint64_t	ull;
    float	f;
    double	d;
    size_t	size;

    mp = &current->metric[hp->midx];

    if (current->result == NULL) {
	/* first time - do not use __pmAllocResult due to realloc requirement */
	current->result = (__pmResult *)calloc(1, sizeof(__pmResult));
	if (current->result == NULL) {
	    pmNoMem("_pmi_stuff_value: result calloc", sizeof(__pmResult), PM_FATAL_ERR);
	}
    }
    rp = current->result;

    pmid = current->metric[hp->midx].pmid;
    for (i = 0; i < rp->numpmid; i++) {
	if (pmid == rp->vset[i]->pmid) {
	    if (mp->desc.indom == PM_INDOM_NULL)
		/* singular metric, cannot have more than one value */
		return PMI_ERR_DUPVALUE;
	    break;
	}
    }
    if (i == rp->numpmid) {
	rp->numpmid++;
	size = sizeof(__pmResult) + (rp->numpmid-1)*sizeof(pmValueSet *);
	rp = current->result = (__pmResult *)realloc(current->result, size);
	if (current->result == NULL) {
	    pmNoMem("_pmi_stuff_value: result realloc", size, PM_FATAL_ERR);
	}
	rp->vset[rp->numpmid-1] = (pmValueSet *)malloc(sizeof(pmValueSet));
	if (rp->vset[rp->numpmid-1] == NULL) {
	    pmNoMem("_pmi_stuff_value: vset alloc", sizeof(pmValueSet), PM_FATAL_ERR);
	}
	vsp = rp->vset[rp->numpmid-1];
	vsp->pmid = pmid;
	vsp->numval = 1;
    }
    else if (rp->vset[i]->numval < 0) {
	/*
	 * This metric is already under an error condition - do
	 * not attempt to add additional instances / values now.
	 */
	return rp->vset[i]->numval;
    }
    else {
	int		j;
	for (j = 0; j < rp->vset[i]->numval; j++) {
	    if (rp->vset[i]->vlist[j].inst == hp->inst)
		/* each metric-instance can appear at most once per pmResult */
		return PMI_ERR_DUPVALUE;
	}
	rp->vset[i]->numval++;
	size = sizeof(pmValueSet) + (rp->vset[i]->numval-1)*sizeof(pmValue);
	vsp = rp->vset[i] = (pmValueSet *)realloc(rp->vset[i], size);
	if (rp->vset[i] == NULL) {
	    pmNoMem("_pmi_stuff_value: vset realloc", size, PM_FATAL_ERR);
	}
    }
    vp = &vsp->vlist[vsp->numval-1];
    vp->inst = hp->inst;
    dsize = -1;
    switch (mp->desc.type) {
	case PM_TYPE_32:
	    vp->value.lval = (__int32_t)strtol(value, &end, 10);
	    if (*end != '\0') {
		if (vsp->numval == 1) vsp->numval = PM_ERR_CONV;
		else rp->vset[i]->numval--;
		return PM_ERR_CONV;
	    }
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_INSITU;
	    break;

	case PM_TYPE_U32:
	    vp->value.lval = (__uint32_t)strtoul(value, &end, 10);
	    if (*end != '\0') {
		if (vsp->numval == 1) vsp->numval = PM_ERR_CONV;
		else rp->vset[i]->numval--;
		return PM_ERR_CONV;
	    }
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_INSITU;
	    break;

	case PM_TYPE_64:
	    ll = strtoint64(value, &end, 10);
	    if (*end != '\0') {
		if (vsp->numval == 1) vsp->numval = PM_ERR_CONV;
		else rp->vset[i]->numval--;
		return PM_ERR_CONV;
	    }
	    dsize = sizeof(ll);
	    data = (void *)&ll;
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_DPTR;
	    break;

	case PM_TYPE_U64:
	    ull = strtouint64(value, &end, 10);
	    if (*end != '\0') {
		if (vsp->numval == 1) vsp->numval = PM_ERR_CONV;
		else rp->vset[i]->numval--;
		return PM_ERR_CONV;
	    }
	    dsize = sizeof(ull);
	    data = (void *)&ull;
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_DPTR;
	    break;

	case PM_TYPE_FLOAT:
	    f = strtof(value, &end);
	    if (*end != '\0') {
		if (vsp->numval == 1) vsp->numval = PM_ERR_CONV;
		else rp->vset[i]->numval--;
		return PM_ERR_CONV;
	    }
	    dsize = sizeof(f);
	    data = (void *)&f;
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_DPTR;
	    break;

	case PM_TYPE_DOUBLE:
	    d = strtod(value, &end);
	    if (*end != '\0') {
		if (vsp->numval == 1) vsp->numval = PM_ERR_CONV;
		else rp->vset[i]->numval--;
		return PM_ERR_CONV;
	    }
	    dsize = sizeof(d);
	    data = (void *)&d;
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_DPTR;
	    break;

	case PM_TYPE_STRING:
	    dsize = strlen(value)+1;
	    data = (void *)value;
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_DPTR;
	    break;

	default:
	    if (vsp->numval == 1) vsp->numval = PM_ERR_TYPE;
	    else rp->vset[i]->numval--;
	    return PM_ERR_TYPE;
    }

    if (dsize != -1) {
	/* logic copied from stuffvalue.c in libpcp */
	int	need = dsize + PM_VAL_HDR_SIZE;

	vp->value.pval = (pmValueBlock *)malloc(need < sizeof(pmValueBlock) ? sizeof(pmValueBlock) : need);
	if (vp->value.pval == NULL) {
	    pmNoMem("_pmi_stuff_value: pmValueBlock", need < sizeof(pmValueBlock) ? sizeof(pmValueBlock) : need, PM_FATAL_ERR);
	}
	vp->value.pval->vlen = (int)need;
	vp->value.pval->vtype = mp->desc.type;
	memcpy((void *)vp->value.pval->vbuf, data, dsize);
    }

    return 0;
}
