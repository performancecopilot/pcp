#include "pmlogreduce.h"

void
doindom(pmResult *rp)
{
    pmValueSet		*vsp;
    int			i;
    int			j;
    int			needti = 0;
    int			need;
    metric_t		*mp;
    int			*instlist;
    char		**namelist;
    int			sts;

    for (i = 0; i < numpmid; i++) {
	if (metriclist[i].idp != NULL) {
	    metriclist[i].idp->state = I_INIT;
	}
    }

    for (i = 0; i < rp->numpmid; i++) {
	vsp = rp->vset[i];
	if (vsp->numval <= 0)
	    continue;
	mp = &metriclist[i];
	if (mp->idp == NULL || mp->idp->state == I_DONE)
	    continue;

	if ((sts = pmGetInDom(mp->idp->indom, &instlist, &namelist)) < 0) {
	    fprintf(stderr,
		"%s: doindom: pmGetInDom (%s) failed: %s\n",
		    pmProgname, pmInDomStr(mp->idp->indom), pmErrStr(sts));
	    exit(1);
	}

	need = 1;
	/*
	 * Need to output the indom if the number of instances changes
	 * or the set of instance ids are not the same from the last
	 * time.
	 */
	if (sts == mp->idp->numinst) {
	    for (j = 0; j < mp->idp->numinst; j++) {
		if (mp->idp->inst[j] != instlist[j])
		    break;
	    }
	    if (j == mp->idp->numinst) {
		/*
		 * Do we need to check the namelist elts as well, e.g.
		 * using strcmp()?
		 * Not at this stage ... if the instance ids are all the
		 * same, then only a very odd (and non-compliant) PMDA
		 * would change the mapping from id to name on the fly
		 */
		need = 0;
	    }
	}

	if (need) {
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "Add metadata: indom %s for metric %s\n", pmInDomStr(mp->idp->indom), pmIDStr(vsp->pmid));
	    }
#endif
	    if (mp->idp->name != NULL) free(mp->idp->name);
	    if (mp->idp->inst != NULL) free(mp->idp->inst);
	    mp->idp->name = namelist;
	    mp->idp->inst = instlist;
	    mp->idp->numinst = sts;
	    if ((sts == __pmLogPutInDom(&logctl, mp->idp->indom, &current, mp->idp->numinst, mp->idp->inst, mp->idp->name)) < 0) {
		fprintf(stderr,
		    "%s: Error: failed to add pmInDom: indom %s (for pmid %s): %s\n",
			pmProgname, pmInDomStr(mp->idp->indom), pmIDStr(vsp->pmid), pmErrStr(sts));
		exit(1);
	    }
	    needti = 1;		/* requires a temporal index update */
	}
	else {
	    free(instlist);
	    free(namelist);
	}

	mp->idp->state = I_DONE;
    }

    if (needti) {
	fflush(logctl.l_mdfp);
	__pmLogPutIndex(&logctl, &current);
    }

}
