#include "pmlogreduce.h"
#include "pcp/archive.h"

int
doindom(__pmResult *rp)
{
    pmValueSet		*vsp;
    int			i;
    int			j;
    int			needti = 0;
    int			need;
    metric_t		*mp = NULL;
    int			*instlist;
    char		**names;
    int			sts;

    for (i = 0; i < rp->numpmid; i++) {
	vsp = rp->vset[i];
	if (vsp->numval <= 0)
	    continue;

	/*
	 * pmidlist[] and rp->vset[]->pmid may not be in 1:1
	 * correspondence because we come here after rewrite() has
	 * been called ... search for matching pmid
	 */
	for (j = 0; j < numpmid; j++) {
	    if (pmidlist[j] == vsp->pmid) {
		mp = &metriclist[j];
		break;
	    }
	}
	if (mp == NULL) {
	    fprintf(stderr,
		"%s: doindom: Arrgh, unexpected PMID %s @ vset[%d]\n",
		    pmGetProgname(), pmIDStr(vsp->pmid), i);
	    __pmPrintResult(stderr, rp);
	    return PM_ERR_GENERIC;
	}
	if (mp->idp == NULL)
	    continue;

	if ((sts = pmGetInDom(mp->idp->indom, &instlist, &names)) < 0) {
	    fprintf(stderr,
		"%s: doindom: pmGetInDom (%s) failed: %s\n",
		    pmGetProgname(), pmInDomStr(mp->idp->indom), pmErrStr(sts));
	    return sts;
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
		 * Do we need to check the 'names' entries as well, e.g.
		 * using strcmp()?
		 * Not at this stage ... if the instance ids are all the
		 * same, then only a very odd (and non-compliant) PMDA
		 * would change the mapping from id to name on the fly
		 */
		need = 0;
	    }
	}

	if (need) {
	    __pmLogInDom	lid;
	    int			pdu_type;
	    if (pmDebugOptions.appl0) {
		fprintf(stderr, "Add metadata: indom %s for metric %s\n", pmInDomStr(mp->idp->indom), pmIDStr(vsp->pmid));
	    }
	    if (mp->idp->name != NULL) free(mp->idp->name);
	    if (mp->idp->inst != NULL) free(mp->idp->inst);
	    lid.indom = mp->idp->indom;
	    lid.stamp = current;	/* struct assignment */
	    lid.numinst = mp->idp->numinst = sts;
	    lid.instlist = mp->idp->inst = instlist;
	    lid.namelist = mp->idp->name = names;
	    lid.alloc = 0;
	    if (__pmLogVersion(archctl.ac_log) >= PM_LOG_VERS03) {
		/* try delta indom */
		pdu_type = TYPE_INDOM;
		sts = pmaTryDeltaInDom(archctl.ac_log, NULL, &lid);
		if (sts < 0) {
		    fprintf(stderr, "Botch: pmaTryDeltaInDom failed: %d\n", sts);
		    return PM_ERR_GENERIC;

		}
		if (sts == 1)
		    pdu_type = TYPE_INDOM_DELTA;
	    }
	    else
		pdu_type = TYPE_INDOM_V2;
	    sts = __pmLogPutInDom(&archctl, pdu_type, &lid);
	    if (pdu_type == TYPE_INDOM_DELTA)
		__pmFreeLogInDom(&lid);
	    if (sts < 0) {
		fprintf(stderr,
		    "%s: Error: failed to add pmInDom: indom %s (for pmid %s): %s\n",
			pmGetProgname(), pmInDomStr(mp->idp->indom), pmIDStr(vsp->pmid), pmErrStr(sts));
		return sts;
	    }
	    needti = 1;		/* requires a temporal index update */
	}
	else {
	    free(instlist);
	    free(names);
	}

    }

    return needti;
}
