#include "pmapi.h"
#include "impl.h"
#include "import.h"
#include "private.h"

int
_pmi_stuff_value(pmi_context *current, pmi_handle *hp, const char *value)
{
    pmResult	*rp;
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

    mp = &current->metric[hp->midx];

    if (current->result == NULL) {
	// first time
	current->result = (pmResult *)malloc(sizeof(pmResult));
	if (current->result == NULL) {
	    __pmNoMem("_pmi_stuff_value: result malloc:", sizeof(pmResult), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	current->result->numpmid = 0;
	current->result->timestamp.tv_sec = 0;
	current->result->timestamp.tv_usec = 0;
    }
    rp = current->result;

    pmid = current->metric[hp->midx].pmid;
    for (i = 0; i < rp->numpmid; i++) {
	if (pmid == rp->vset[i]->pmid) {
	    if (mp->desc.indom == PM_INDOM_NULL)
		// singular metric, cannot have more than one value
		return PMI_ERR_DUPVALUE;
	    break;
	}
    }
    if (i == rp->numpmid) {
	rp->numpmid++;
	rp = current->result = (pmResult *)realloc(current->result, sizeof(pmResult) + (rp->numpmid - 1)*sizeof(pmValueSet *));
	if (current->result == NULL) {
	    __pmNoMem("_pmi_stuff_value: result realloc:", sizeof(pmResult) + (rp->numpmid - 1)*sizeof(pmValueSet *), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	rp->vset[rp->numpmid-1] = (pmValueSet *)malloc(sizeof(pmValueSet));
	if (rp->vset[rp->numpmid-1] == NULL) {
	    __pmNoMem("_pmi_stuff_value: vset alloc:", sizeof(pmValueSet), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	vsp = rp->vset[rp->numpmid-1];
	vsp->pmid = pmid;
	vsp->numval = 1;
    }
    else {
	int		j;
	for (j = 0; j < rp->vset[i]->numval; j++) {
	    if (rp->vset[i]->vlist[j].inst == hp->inst)
		// each metric-instance can appear at most once per pmResult
		return PMI_ERR_DUPVALUE;
	}
	rp->vset[i]->numval++;
	vsp = rp->vset[i] = (pmValueSet *)realloc(rp->vset[i], sizeof(pmValueSet) + (rp->vset[i]->numval-1)*sizeof(pmValue));
	if (rp->vset[i] == NULL) {
	    __pmNoMem("_pmi_stuff_value: vset realloc:", sizeof(pmValueSet) + (rp->vset[i]->numval-1)*sizeof(pmValue), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
    }
    vp = &vsp->vlist[vsp->numval-1];
    vp->inst = hp->inst;
    dsize = -1;
    switch (mp->desc.type) {
	case PM_TYPE_32:
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_INSITU;
	    vp->value.lval = strtol(value, &end, 10);
	    if (*end != '\0') {
		vsp->numval = PM_ERR_CONV;
		return PM_ERR_CONV;
	    }
	    break;

	case PM_TYPE_U32:
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_INSITU;
	    vp->value.lval = strtoul(value, &end, 10);
	    if (*end != '\0') {
		vsp->numval = PM_ERR_CONV;
		return PM_ERR_CONV;
	    }
	    break;

	case PM_TYPE_64:
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_DPTR;
	    ll = strtoll(value, &end, 10);
	    if (*end != '\0') {
		vsp->numval = PM_ERR_CONV;
		return PM_ERR_CONV;
	    }
	    dsize = sizeof(ll);
	    data = (void *)&ll;
	    break;

	case PM_TYPE_U64:
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_DPTR;
	    ull = strtoull(value, &end, 10);
	    if (*end != '\0') {
		vsp->numval = PM_ERR_CONV;
		return PM_ERR_CONV;
	    }
	    dsize = sizeof(ull);
	    data = (void *)&ull;
	    break;

	case PM_TYPE_FLOAT:
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_DPTR;
	    f = strtof(value, &end);
	    if (*end != '\0') {
		vsp->numval = PM_ERR_CONV;
		return PM_ERR_CONV;
	    }
	    dsize = sizeof(f);
	    data = (void *)&f;
	    break;

	case PM_TYPE_DOUBLE:
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_DPTR;
	    d = strtod(value, &end);
	    if (*end != '\0') {
		vsp->numval = PM_ERR_CONV;
		return PM_ERR_CONV;
	    }
	    dsize = sizeof(d);
	    data = (void *)&d;
	    break;

	case PM_TYPE_STRING:
	    if (vsp->numval == 1) vsp->valfmt = PM_VAL_DPTR;
	    dsize = strlen(value)+1;
	    data = (void *)value;
	    break;

	default:
	    vsp->numval = PM_ERR_TYPE;
	    return PM_ERR_TYPE;
    }

    if (dsize != -1) {
	// logic copied from stuffvalue.c in libpcp
	//
	int	need = dsize + PM_VAL_HDR_SIZE;

	if (dsize == sizeof(__int64_t))
	    vp->value.pval = (pmValueBlock *)__pmPoolAlloc(need);
	else
	    vp->value.pval = (pmValueBlock *)malloc(need < sizeof(pmValueBlock) ? sizeof(pmValueBlock) : need);
	if (vp->value.pval == NULL) {
	    __pmNoMem("_pmi_stuff_value: pmValueBlock:", need < sizeof(pmValueBlock) ? sizeof(pmValueBlock) : need, PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	vp->value.pval->vlen = (int)need;
	vp->value.pval->vtype = mp->desc.type;
	memcpy((void *)vp->value.pval->vbuf, data, dsize);
    }

    return 0;
}
