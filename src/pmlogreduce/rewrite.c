#include "pmlogreduce.h"
#include <assert.h>

static pmResult	*orp = NULL;

/*
 * Must either re-write the pmResult, or report a fatal error and
 * return NULL
 */
pmResult *
rewrite(pmResult *rp)
{
    int			i;
    int			sts;

    if ((orp = (pmResult *)malloc(sizeof(pmResult) +
			(rp->numpmid - 1) * sizeof(pmValueSet *))) == NULL) {
	fprintf(stderr,
		"%s: rewrite: cannot malloc pmResult for %d metrics\n",
		    pmProgname, rp->numpmid);
	    return NULL;
    }
    orp->numpmid = 0;
    orp->timestamp = rp->timestamp;	/* struct assignment */

    for (i = 0; i < rp->numpmid; i++) {
	metric_t	*mp;
	value_t		*vp;
	pmValueSet	*vsp = rp->vset[i];
	pmValueSet	*ovsp;
	int		j;
	int		need;

	assert(pmidlist[i] = vsp->pmid);

	if (vsp->numval > 0)
	    need = (vsp->numval - 1) * sizeof(pmValue);
	else
	    need = 0;
	ovsp = (pmValueSet *)malloc(sizeof(pmValueSet) +
				need*sizeof(pmValue));
	if (ovsp == NULL) {
	    fprintf(stderr,
		"%s: rewrite: Arrgh, cannot malloc %d bytes for osvp\n",
		    pmProgname, sizeof(pmValueSet) + need * sizeof(pmValue));
	    exit(1);
	    /*NOTREACHED*/
	}
	ovsp->pmid = vsp->pmid;
	ovsp->valfmt = vsp->valfmt;
	if (vsp->numval <= 0) {
	    ovsp->numval = vsp->numval;
	    orp->vset[orp->numpmid] = ovsp;
	    orp->numpmid++;
	}
	else {
	    ovsp->numval = 0;
	    mp = &metriclist[i];
	    for (j = 0; j < vsp->numval; j++) {
		for (vp = mp->first; vp != NULL; vp = vp->next) {
		    if (vp->inst == vsp->vlist[j].inst)
			break;
		}
		if (vp == NULL) {
		    fprintf(stderr,
			"%s: rewrite: Arrgh: cannot find inst %d in value_t list for %s (%s)\n",
			    pmProgname, vsp->vlist[j].inst, namelist[i], pmIDStr(vsp->pmid));
		    exit(1);
		    /*NOTREACHED*/
		}
		if ((vp->control & (V_SEEN|V_INIT)) == 0)
		    continue;
		/*
		 * we've seen this metric-instance pair in the last
		 * interval, or it is the first time for this one
		 */
		if (mp->rewrite) {
		    pmAtomValue	av;
		    int		k;
		    sts = pmExtractValue(vsp->valfmt, &vsp->vlist[j], mp->idesc.type, &av, mp->odesc.type);
		    if (sts < 0) {
			fprintf(stderr,
			    "%s: rewrite: pmExtractValue failed for pmid %s value %d: %s\n",
				pmProgname, pmIDStr(vsp->pmid), j, pmErrStr(sts));
			exit(1);
			/*NOTREACHED*/
		    }
		    ovsp->pmid = vsp->pmid;
		    ovsp->vlist[ovsp->numval].inst = vsp->vlist[j].inst;
		    k = __pmStuffValue(&av, 0, &ovsp->vlist[ovsp->numval], mp->odesc.type);
		    if (k < 0) {
			fprintf(stderr,
			    "%s: rewrite: __pmStuffValue failed for pmid %s value %d: %s\n",
				pmProgname, pmIDStr(vsp->pmid), j, pmErrStr(sts));
			exit(1);
			/*NOTREACHED*/
		    }
		    if (ovsp->numval == 0)
			ovsp->valfmt = k;
		    ovsp->numval++;
		    vp->timestamp = rp->timestamp;
		    vp->value = av;
		}
		else {
		    ovsp->vlist[ovsp->numval] = vsp->vlist[j];
		    ovsp->numval++;
		}
		vp->control &= ~V_INIT;
	    }
	    if (ovsp->numval > 0) {
		orp->vset[orp->numpmid] = ovsp;
		orp->numpmid++;
	    }
	    else
		free(ovsp);
	}
    }

    /* TODO ... orp->numpmid == 0? */

    return orp;
}

void
rewrite_free(void)
{
    int			i;

    if (orp == NULL)
	return;

    for (i = 0; i < orp->numpmid; i++) {
	pmValueSet	*vsp = orp->vset[i];
	int		j;

	if (vsp->numval <= 0)
	    continue;

	if (metriclist[i].rewrite) {
	    for (j = 0; j < vsp->numval; j++) {
		free(vsp->vlist[j].value.pval);
	    }
	}

	free(orp->vset[i]);
    }

    free(orp);
    orp = NULL;
}
