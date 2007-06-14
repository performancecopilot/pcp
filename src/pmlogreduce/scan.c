#include "pmlogreduce.h"

static struct timeval	last_tv = { 0, 0 };
static int		ictx_b = -1;

extern struct timeval	winstart_tval;

/*
 * This is the heart of the data reduction algorithm.  The term
 * metric-instance is used here to reflect the fact that this computation
 * has to be performed for every instance of every metric.
 *
 * 1. need to look at every input archive record going forward from the
 *    current point up to the last one <= end (time) ... so no interp
 *    mode here
 *
 * 2. for counter metric-instances, look for and count "wraps"
 *
 * 3. for instantenous or discrete metric-instances with a numeric type,
 *    compute the arithmetic average of the observations over the
 *    interval
 *
 * 4. for _all_ metric-instances if there are no observations in the
 *    interval, then we'd like to supress this metric-instance from the
 *    output archive
 *
 * 5. all of the above has to be done in a way that makes sense in the
 *    presence of mark records
 */

void
doscan(struct timeval *end)
{
    pmResult		*rp;
    value_t		*vp;
    int			sts;
    int			i;
    int			ir;
    int			nr;

    if (ictx_b == -1) {
	/*
	 * first time, create the record at a time mode context for the
	 * input archive
	 */
	if ((ictx_b = pmNewContext(PM_CONTEXT_ARCHIVE, iname)) < 0) {
	    fprintf(stderr, "%s: Error: cannot open archive \"%s\" (ctx_b): %s\n",
		    pmProgname, iname, pmErrStr(ictx_b));
	    exit(1);
	    /*NOTREACHED*/
	}

	if ((sts = pmSetMode(PM_MODE_FORW, NULL, 0)) < 0) {
	    fprintf(stderr,
		"%s: Error: pmSetMode (ictx_b) failed: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	    /*NOTREACHED*/
	}
    }

    if ((sts = pmUseContext(ictx_b)) < 0) {
	fprintf(stderr, "%s: doscan: Error: cannot use context: %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
	/*NOTREACHED*/
    }

    for (i = 0; i < numpmid; i++) {
	for (vp = metriclist[i].first; vp != NULL; vp = vp->next) {
	    vp->nobs = vp->nwrap = 0;
	    vp->control &= ~V_SEEN;
	}
    }

    for (nr = 0; ; nr++) {

	if ((sts = pmFetchArchive(&rp)) < 0) {
	    if (sts == PM_ERR_EOL)
		break;
	    fprintf(stderr,
		"%s: doscan: Error: pmFetch failed: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	    /*NOTREACHED*/
	}
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2) {
	    if (nr == 0) {
		fprintf(stderr, "scan starts at ");
		__pmPrintStamp(stderr, &rp->timestamp);
		fprintf(stderr, "\n");
	    }
	}
#endif
	if (rp->timestamp.tv_sec > end->tv_sec ||
	    (rp->timestamp.tv_sec == end->tv_sec &&
	     rp->timestamp.tv_usec > end->tv_usec)) {
	    /*
	     * past the end of the interval, remember timestamp so we
	     * can resume here next time
	     */
	    last_tv = rp->timestamp;	/* struct assignment */
	    pmFreeResult(rp);
	    break;
	}

	for (ir = 0; ir < rp->numpmid; ir++) {
	    pmValueSet		*vsp;
	    int			j;
	    metric_t		*mp;

	    vsp = rp->vset[ir];
	    if (vsp->numval <= 0)
		continue;

	    for (i = 0; i < numpmid; i++) {
		if (vsp->pmid == pmidlist[i])
		    break;
	    }
	    if (i == numpmid) {
		fprintf(stderr,
		    "%s: scan: Arrgh, cannot find pid %s in pidlist[]\n",
			pmProgname, pmIDStr(vsp->pmid));
		exit(1);
		/*NOTREACHED*/
	    }
	    mp = &metriclist[i];

	    for (j = 0; j < vsp->numval; j++) {
		value_t		*lvp = NULL;
		for (vp = mp->first; vp != NULL; vp = vp->next) {
		    if (vp->inst == vsp->vlist[j].inst)
			break;
		    lvp = vp;
		}
		if (vp == NULL) {
		    vp = (value_t *)malloc(sizeof(value_t));
		    if (vp == NULL) {
			fprintf(stderr,
			    "%s: rewrite: Arrgh, cannot malloc value_t\n", pmProgname);
			exit(1);
			/*NOTREACHED*/
		    }
		    if (lvp == NULL)
			mp->first = vp;
		    else
			lvp->next = vp;
		    vp->inst = vsp->vlist[j].inst;
		    vp->nobs = vp->nwrap = 0;
		    vp->control = V_INIT;
		    vp->next = NULL;
#if PCP_DEBUG && DESPERATE

		    if (pmDebug & DBG_TRACE_APPL0) {
			fprintf(stderr,
			    "add value_t for %s (%s) inst %d\n",
			    namelist[i], pmIDStr(pmidlist[i]), vsp->vlist[j].inst);
		    }
#endif
		}
		/* TODO ... hard part goes here 8^) */
		if (mp->idesc.sem == PM_SEM_COUNTER) {
		    /*
		     * OK, this metric is a counter, scan each instance
		     * looking for potential wraps
		     */
		    ;
		}
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL1) {
		    __pmPrintStamp(stderr, &rp->timestamp);
		    fprintf(stderr, ": seen %s (%s) inst %d\n",
			namelist[i], pmIDStr(pmidlist[i]),
			vsp->vlist[j].inst);
		}
#endif
		vp->control |= V_SEEN;
	    }
	}

	pmFreeResult(rp);
    }
#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "scan ends at ");
	__pmPrintStamp(stderr, &last_tv);
	if (sts == PM_ERR_EOL)
	    fprintf(stderr, " [EOL]");
	fprintf(stderr, " (%d records)\n", nr);
    }
#endif

    if ((sts == pmSetMode(PM_MODE_FORW, &last_tv, 0)) < 0) {
	fprintf(stderr,
	    "%s: doscan: Error: pmSetMode (ictx_b) time=", pmProgname);
	__pmPrintStamp(stderr, &last_tv);
	fprintf(stderr,
	    " failed: %s\n", pmErrStr(sts));
	exit(1);
	/*NOTREACHED*/
    }
}
