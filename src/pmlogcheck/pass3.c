/*
 * Copyright (c) 2013 Ken McDonell, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 */

#include <math.h>
#include "pmapi.h"
#include "impl.h"
#include "logcheck.h"

typedef struct {
    int			inst;
    double		lastval;	/* value from previous sample */
    struct timeval	lasttime;	/* time of previous sample */
} instData;

typedef struct {
    pmDesc		desc;
    int			valfmt;
    double		scale;
    instData		**instlist;
    unsigned int	listsize;
} checkData;

static __pmHashCtl	hashlist;	/* hash statistics about each metric */
static int		dayflag;

static __pmContext	*l_ctxp;
static char		*l_archname;

/* time manipulation */
static int
tsub(struct timeval *a, struct timeval *b)
{
    if ((a == NULL) || (b == NULL))
	return -1;
    __pmtimevalDec(a, b);
    return 0;
}

static char *
typeStr(int pmtype)
{
    switch (pmtype) {
	case PM_TYPE_32:
	    return "signed 32-bit";
	case PM_TYPE_U32:
	    return "unsigned 32-bit";
	case PM_TYPE_64:
	    return "signed 64-bit";
	case PM_TYPE_U64:
	    return "unsigned 64-bit";
    }
    return "unknown type";
}

static void
print_metric(FILE *f, pmID pmid)
{
    int		numnames;
    char	**names;

    if ((numnames = pmNameAll(pmid, &names)) < 1)
	fprintf(f, "%s", pmIDStr(pmid));
    else {
	__pmPrintMetricNames(f, numnames, names, " or ");
	free(names);
    }
}

static void
print_stamp(FILE *f, struct timeval *stamp)
{
    if (dayflag) {
	char	*ddmm;
	char	*yr;
	char	timebuf[32];	/* for pmCtime result + .xxx */

	ddmm = pmCtime(&stamp->tv_sec, timebuf);
	ddmm[10] = ' ';
	ddmm[11] = '\0';
	yr = &ddmm[20];
	fprintf(f, "%s", ddmm);
	__pmPrintStamp(f, stamp);
	fprintf(f, " %4.4s", yr);
    }
    else
	__pmPrintStamp(f, stamp);
}

static double
unwrap(double current, struct timeval *curtime, checkData *checkdata, int index)
{
    double	outval = current;
    int		wrapflag = 0;
    char	*str = NULL;

    if ((current - checkdata->instlist[index]->lastval) < 0.0 &&
        checkdata->instlist[index]->lasttime.tv_sec > 0) {
	switch (checkdata->desc.type) {
	    case PM_TYPE_32:
	    case PM_TYPE_U32:
		outval += (double)UINT_MAX+1;
		wrapflag = 1;
		break;
	    case PM_TYPE_64:
	    case PM_TYPE_U64:
		outval += (double)ULONGLONG_MAX+1;
		wrapflag = 1;
		break;
	    default:
		wrapflag = 0;
	}
    }

    if (wrapflag) {
	fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
	print_stamp(stderr, curtime);
	fprintf(stderr, "]: ");
	print_metric(stderr, checkdata->desc.pmid);
	if (pmNameInDom(checkdata->desc.indom, checkdata->instlist[index]->inst, &str) < 0)
	    fprintf(stderr, ": %s wrap", typeStr(checkdata->desc.type));
	else {
	    fprintf(stderr, "[%s]: %s wrap", str, typeStr(checkdata->desc.type));
	    free(str);
	}
	fprintf(stderr, "\n\tvalue %.0f at ", checkdata->instlist[index]->lastval);
	print_stamp(stderr, &checkdata->instlist[index]->lasttime);
	fprintf(stderr, "\n\tvalue %.0f at ", current);
	print_stamp(stderr, curtime);
	fputc('\n', stderr);
    }

    return outval;
}

static void
newHashInst(pmValue *vp,
	checkData *checkdata,		/* updated by this function */
	int valfmt,
	struct timeval *timestamp,	/* timestamp for this sample */
	int pos)			/* position of this inst in instlist */
{
    int		sts;
    size_t	size;
    pmAtomValue av;

    if ((sts = pmExtractValue(valfmt, vp, checkdata->desc.type, &av, PM_TYPE_DOUBLE)) < 0) {
	fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
	print_stamp(stderr, timestamp);
	fprintf(stderr, "] ");
	print_metric(stderr, checkdata->desc.pmid);
	fprintf(stderr, ": pmExtractValue failed: %s\n", pmErrStr(sts));
	fprintf(stderr, "%s: possibly corrupt archive?\n", pmProgname);
	exit(EXIT_FAILURE);
    }
    size = (pos+1)*sizeof(instData*);
    checkdata->instlist = (instData**) realloc(checkdata->instlist, size);
    if (!checkdata->instlist)
	__pmNoMem("newHashInst.instlist", size, PM_FATAL_ERR);
    size = sizeof(instData);
    checkdata->instlist[pos] = (instData*) malloc(size);
    if (!checkdata->instlist[pos])
	__pmNoMem("newHashInst.instlist[pos]", size, PM_FATAL_ERR);
    checkdata->instlist[pos]->inst = vp->inst;
    checkdata->instlist[pos]->lastval = av.d;
    checkdata->instlist[pos]->lasttime = *timestamp;
    checkdata->listsize++;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	char	*name;

	fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
	print_stamp(stderr, timestamp);
	fprintf(stderr, "] ");
	print_metric(stderr, checkdata->desc.pmid);
	if (vp->inst == PM_INDOM_NULL)
	    fprintf(stderr, ": new singular metric\n");
	else {
	    fprintf(stderr, ": new metric-instance pair ");
	    if (pmNameInDom(checkdata->desc.indom, vp->inst, &name) < 0)
		fprintf(stderr, "%d\n", vp->inst);
	    else {
		fprintf(stderr, "\"%s\"\n", name);
		free(name);
	    }
	}

    }
#endif
}

static void
newHashItem(pmValueSet *vsp,
	pmDesc *desc,
	checkData *checkdata,		/* output from this function */
	struct timeval *timestamp)	/* timestamp for this sample */
{
    int j;

    checkdata->desc = *desc;
    checkdata->scale = 0.0;

    /* convert counter metric units to rate units & get time scale */
    if (checkdata->desc.sem == PM_SEM_COUNTER) {
	if (checkdata->desc.units.dimTime == 0)
	    checkdata->scale = 1.0;
	else {
	    if (checkdata->desc.units.scaleTime > PM_TIME_SEC)
		checkdata->scale = pow(60.0, (double)(PM_TIME_SEC - checkdata->desc.units.scaleTime));
	    else
		checkdata->scale = pow(1000.0, (double)(PM_TIME_SEC - checkdata->desc.units.scaleTime));
	}
	if (checkdata->desc.units.dimTime == 0) checkdata->desc.units.scaleTime = PM_TIME_SEC;
	checkdata->desc.units.dimTime--;
    }

    checkdata->listsize = 0;
    checkdata->instlist = NULL;
    for (j = 0; j < vsp->numval; j++) {
	newHashInst(&vsp->vlist[j], checkdata, vsp->valfmt, timestamp, j);
    }
}

static void
docheck(pmResult *result)
{
    int			i, j, k;
    int			sts;
    pmDesc		desc;
    pmAtomValue 	av;
    pmValue		*vp;
    pmValueSet		*vsp;
    __pmHashNode	*hptr = NULL;
    checkData		*checkdata = NULL;
    double		diff;
    struct timeval	timediff;

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1) {
	    if (vsp->numval == 0) {
		fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
		print_stamp(stderr, &result->timestamp);
		fprintf(stderr, "] ");
		print_metric(stderr, vsp->pmid);
		fprintf(stderr, ": no values returned\n");
		continue;
	    }
	    else if (vsp->numval < 0) {
		fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
		print_stamp(stderr, &result->timestamp);
		fprintf(stderr, "] ");
		print_metric(stderr, vsp->pmid);
		fprintf(stderr, ": error from numval: %s\n", pmErrStr(vsp->numval));
		continue;
	    }
	}
#endif
	if (vsp->numval <= 0)
	    continue;

	/* check if pmid already in hash list */
	if ((hptr = __pmHashSearch(vsp->pmid, &hashlist)) == NULL) {
	    if ((sts = pmLookupDesc(vsp->pmid, &desc)) < 0) {
		fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
		print_stamp(stderr, &result->timestamp);
		fprintf(stderr, "] ");
		print_metric(stderr, vsp->pmid);
		fprintf(stderr, ": pmLookupDesc failed: %s\n", pmErrStr(sts));
		continue;
	    }

	    if (desc.type != PM_TYPE_32 && desc.type != PM_TYPE_U32 &&
		desc.type != PM_TYPE_64 && desc.type != PM_TYPE_U64 &&
		desc.type != PM_TYPE_FLOAT && desc.type != PM_TYPE_DOUBLE) {
		continue;	/* no checks for non-numeric metrics */
	    }

	    /* create a new one & add to list */
	    checkdata = (checkData*) malloc(sizeof(checkData));
	    newHashItem(vsp, &desc, checkdata, &result->timestamp);
	    if (vsp->numval > 0)
		checkdata->valfmt = vsp->valfmt;
	    else
		checkdata->valfmt = -1;
	    if (__pmHashAdd(checkdata->desc.pmid, (void*)checkdata, &hashlist) < 0) {
		fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
		print_stamp(stderr, &result->timestamp);
		fprintf(stderr, "] ");
		print_metric(stderr, vsp->pmid);
		fprintf(stderr, ": __pmHashAdd failed (internal pmlogcheck error)\n");
		/* free memory allocated above on insert failure */
		for (j = 0; j < vsp->numval; j++) {
		    if (checkdata->instlist[j] != NULL)
			free(checkdata->instlist[j]);
		}
		if (checkdata->instlist != NULL)
		    free(checkdata->instlist);
		continue;
	    }
	}
	else {	/* pmid exists - update statistics */
	    checkdata = (checkData *)hptr->data;
	    if (vsp->numval > 0) {
		if (checkdata->valfmt == -1)
		    checkdata->valfmt = vsp->valfmt;
		else if (checkdata->valfmt != vsp->valfmt) {
		    /*
		     * this is not supposed to happen ... when values
		     * are present valfmt should be the same for all
		     * pmValueSets for a given PMID
		     */
		    fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
		    print_stamp(stderr, &result->timestamp);
		    fprintf(stderr, "] ");
		    print_metric(stderr, vsp->pmid);
		    fprintf(stderr, ": encoding botch valfmt=%d not %d as expected\n", vsp->valfmt, checkdata->valfmt);
		    continue;
		}
	    }
	    for (j = 0; j < vsp->numval; j++) {	/* iterate thro result values */
		vp = &vsp->vlist[j];
		k = j;	/* index into stored inst list, result may differ */
		if ((vsp->numval > 1) || (checkdata->desc.indom != PM_INDOM_NULL)) {
		    /* must store values using correct inst - probably in correct order already */
		    if ((k < checkdata->listsize) && (checkdata->instlist[k]->inst != vp->inst)) {
			for (k = 0; k < checkdata->listsize; k++) {
			    if (vp->inst == checkdata->instlist[k]->inst) {
				break;	/* k now correct */
			    }
			}
			if (k == checkdata->listsize) {	/* no matching inst was found */
			    newHashInst(vp, checkdata, vsp->valfmt, &result->timestamp, k);
			    continue;
			}
		    }
		    else if (k >= checkdata->listsize) {
			k = checkdata->listsize;
			newHashInst(vp, checkdata, vsp->valfmt, &result->timestamp, k);
			continue;
		    }
		}
		if (k >= checkdata->listsize) {	/* only error values observed so far */
		    k = checkdata->listsize;
		    newHashInst(vp, checkdata, vsp->valfmt, &result->timestamp, k);
		    continue;
		}

		timediff = result->timestamp;
		tsub(&timediff, &(checkdata->instlist[k]->lasttime));
		if (timediff.tv_sec < 0) {
		    /* clip negative values at zero */
		    timediff.tv_sec = 0;
		    timediff.tv_usec = 0;
		}
		diff = __pmtimevalToReal(&timediff);
		if ((sts = pmExtractValue(vsp->valfmt, vp, checkdata->desc.type, &av, PM_TYPE_DOUBLE)) < 0) {
		    fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
		    print_stamp(stderr, &result->timestamp);
		    fprintf(stderr, "] ");
		    print_metric(stderr, vsp->pmid);
		    fprintf(stderr, ": pmExtractValue failed: %s\n", pmErrStr(sts));
		    continue;
		}
		if (checkdata->desc.sem == PM_SEM_COUNTER) {
		    if (diff == 0.0) continue;
		    diff *= checkdata->scale;
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL2) {
			fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
			print_stamp(stderr, &result->timestamp);
			fprintf(stderr, "] ");
			print_metric(stderr, checkdata->desc.pmid);
			fprintf(stderr, ": current counter value is %.0f\n", av.d);
		    }
#endif
		    unwrap(av.d, &(result->timestamp), checkdata, k);
		}
		checkdata->instlist[k]->lastval = av.d;
		checkdata->instlist[k]->lasttime = result->timestamp;
	    }
	}
    }
}

int
pass3(__pmContext *ctxp, char *archname, pmOptions *opts)
{
    struct timeval	timespan;
    int			sts;
    pmResult		*result;
    struct timeval	last_stamp;
    struct timeval	delta_stamp;

    l_ctxp = ctxp;
    l_archname = archname;

    if (vflag)
	fprintf(stderr, "%s: start pass3\n", archname);
    
    if ((sts = pmSetMode(PM_MODE_FORW, &opts->start, 0)) < 0) {
	fprintf(stderr, "%s: pmSetMode failed: %s\n", l_archname, pmErrStr(sts));
	return STS_FATAL;
    }

    /* check which timestamp print format we should be using */
    timespan = opts->finish;
    tsub(&timespan, &opts->start);
    if (timespan.tv_sec > 86400) /* seconds per day: 60*60*24 */
	dayflag = 1;

    sts = 0;
    last_stamp = opts->start;
    for ( ; ; ) {
	/*
	 * we need the next record with no fancy checks or record
	 * skipping in libpcp, so use __pmLogRead() in preference
	 * to pmFetchArchive()
	 */
	if ((sts = __pmLogRead(l_ctxp->c_archctl->ac_log, l_ctxp->c_mode, NULL, &result, PMLOGREAD_NEXT)) < 0)
	    break;
	result_count++;
	delta_stamp = result->timestamp;
	tsub(&delta_stamp, &last_stamp);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    int		i;
	    int		sum_val = 0;
	    int		cnt_noval = 0;
	    int		cnt_err = 0;
	    pmValueSet	*vsp;

	    fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
	    print_stamp(stderr, &result->timestamp);
	    for (i = 0; i < result->numpmid; i++) {
		vsp = result->vset[i];
		if (vsp->numval > 0)
		    sum_val += vsp->numval;
		else if (vsp->numval == 0)
		    cnt_noval++;
		else
		    cnt_err++;
	    }
	    fprintf(stderr, "] delta(stamp)=%.3fsec", __pmtimevalToReal(&delta_stamp));
	    fprintf(stderr, " numpmid=%d sum(numval)=%d", result->numpmid, sum_val);
	    if (cnt_noval > 0)
		fprintf(stderr, " count(numval=0)=%d", cnt_noval);
	    if (cnt_err > 0)
		fprintf(stderr, " count(numval<0)=%d", cnt_err);
	    fputc('\n', stderr);
	}
#endif
	if (delta_stamp.tv_sec < 0) {
	    /* time went backwards! */
	    fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
	    print_stamp(stderr, &result->timestamp);
	    fprintf(stderr, "]: timestamp went backwards, prior timestamp: ");
	    print_stamp(stderr, &last_stamp);
	    fprintf(stderr, "\n");
	}

	last_stamp = result->timestamp;
	if ((opts->finish.tv_sec > result->timestamp.tv_sec) ||
	    ((opts->finish.tv_sec == result->timestamp.tv_sec) &&
	     (opts->finish.tv_usec >= result->timestamp.tv_usec))) {
	    if (result->numpmid == 0) {
		/*
		 * MARK record ... make sure wrap check is not done
		 * at next fetch (mimic interp.c from libpcp)
		 */
		__pmHashNode	*hptr;
		checkData	*checkdata;
		int		k;
		for (hptr = __pmHashWalk(&hashlist, PM_HASH_WALK_START);
		     hptr != NULL;
		     hptr = __pmHashWalk(&hashlist, PM_HASH_WALK_NEXT)) {
		    checkdata = (checkData *)hptr->data;
		    for (k = 0; k < checkdata->listsize; k++) {
			checkdata->instlist[k]->lasttime.tv_sec = 0;
		    }
		}

		mark_count++;
	    }
	    else
		docheck(result);
	    pmFreeResult(result);
	}
	else {
	    pmFreeResult(result);
	    sts = PM_ERR_EOL;
	    break;
	}
    }
    if (sts != PM_ERR_EOL) {
	fprintf(stderr, "[after ");
	print_stamp(stderr, &last_stamp);
	fprintf(stderr, "]: pmFetch: error: %s\n", pmErrStr(sts));
	return STS_FATAL;
    }

    return STS_OK;
}
