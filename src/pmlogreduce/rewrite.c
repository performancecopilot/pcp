#include "pmlogreduce.h"
#include <inttypes.h>

static pmResult	*orp;

/*
 * Must either re-write the pmResult, or return NULL for non-fatal
 * errors, else report and exit for catastrophic errors ...
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
		    pmGetProgname(), rp->numpmid);
	    exit(1);
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

	if (pmidlist[i] != vsp->pmid) {
	    fprintf(stderr,
		"%s: rewrite: Arrgh, mismatched PMID %s vs %s\n",
		    pmGetProgname(), pmIDStr(pmidlist[i]), pmIDStr(vsp->pmid));
	    exit(1);
	}

	if (vsp->numval > 0)
	    need = (vsp->numval - 1) * sizeof(pmValue);
	else
	    need = 0;
	ovsp = (pmValueSet *)malloc(sizeof(pmValueSet) +
				need*sizeof(pmValue));
	if (ovsp == NULL) {
	    __uint64_t bytes = (sizeof(pmValueSet) + need * sizeof(pmValue));
	    fprintf(stderr,
		"%s: rewrite: Arrgh, cannot malloc %"PRIi64" bytes for ovsp\n",
		    pmGetProgname(), bytes);
	    exit(1);
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
	    if (mp->mode != MODE_SKIP) {
		for (j = 0; j < vsp->numval; j++) {
		    for (vp = mp->first; vp != NULL; vp = vp->next) {
			if (vp->inst == vsp->vlist[j].inst)
			    break;
		    }
		    if (vp == NULL) {
			fprintf(stderr,
			    "%s: rewrite: Arrgh: cannot find inst %d in value_t list for %s (%s)\n",
				pmGetProgname(), vsp->vlist[j].inst, namelist[i], pmIDStr(vsp->pmid));
			exit(1);
		    }
		    if ((vp->control & (V_SEEN|V_INIT)) == 0)
			continue;
		    /*
		     * we've seen this metric-instance pair in the last
		     * interval, or it is the first time for this one
		     */
		    if (mp->mode == MODE_REWRITE) {
			pmAtomValue	av;
			int		k;
			sts = pmExtractValue(vsp->valfmt, &vsp->vlist[j], mp->idesc.type, &av, mp->odesc.type);
			if (sts < 0) {
			    fprintf(stderr,
				"%s: rewrite: pmExtractValue failed for pmid %s value %d: %s\n",
				    pmGetProgname(), pmIDStr(vsp->pmid), j, pmErrStr(sts));
			    exit(1);
			}
			ovsp->pmid = vsp->pmid;
			ovsp->vlist[ovsp->numval].inst = vsp->vlist[j].inst;
			k = __pmStuffValue(&av, &ovsp->vlist[ovsp->numval], mp->odesc.type);
			if (k < 0) {
			    fprintf(stderr,
				"%s: rewrite: __pmStuffValue failed for pmid %s value %d: %s\n",
				    pmGetProgname(), pmIDStr(vsp->pmid), j, pmErrStr(sts));
			    exit(1);
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
	    }
	    if (ovsp->numval > 0) {
		orp->vset[orp->numpmid] = ovsp;
		orp->numpmid++;
	    }
	    else
		free(ovsp);
	}
    }

    if (orp->numpmid == 0) {
	/*
	 * very unlikely that all metrics are either skipped or have
	 * no values, but it might happen ... do not allow this record
	 * to be written because it looks like a "mark" record with
	 * numpmid == 0
	 */
	free(orp);
	orp = NULL;
    }

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
	metric_t	*mp;

	for (j = 0; j < numpmid; j++) {
	    if (vsp->pmid == pmidlist[j])
		break;
	}
	if (j == numpmid) {
	    fprintf(stderr,
		"%s: rewrite_free: Arrgh, cannot find pmid %s in pmidlist[]\n",
		    pmGetProgname(), pmIDStr(vsp->pmid));
	    exit(1);
	}
	mp = &metriclist[j];

	if (vsp->numval > 0 && mp->mode == MODE_REWRITE) {
	    /*
	     * MODE_REWRITE implies the value was promoted to 64-bit
	     * and the pval in the pmResult came from the __pmStuffValue()
	     * call above, so free it here
	     */
	    for (j = 0; j < vsp->numval; j++) {
		free(vsp->vlist[j].value.pval);
	    }
	}

	free(vsp);
    }

    free(orp);
    orp = NULL;
}
