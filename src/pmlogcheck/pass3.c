/*
 * Copyright (c) 2013 Ken McDonell, Inc.  All Rights Reserved.
 * Copyright (c) 2022 Red Hat.
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
 */

#include <math.h>
#include "pmapi.h"
#include "libpcp.h"
#include "logcheck.h"

typedef struct {
    int			inst;
    double		lastval;	/* value from previous sample */
    __pmTimestamp	lasttime;	/* time of previous sample */
} instData;

typedef struct {
    pmDesc		desc;
    int			valfmt;
    double		scale;
    __pmHashCtl		insthash;
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
    pmtimevalDec(a, b);
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
print_stamp_msec(FILE *f, __pmTimestamp *tp)
{
    struct tm   tmp;
    time_t      now;

    now = (time_t)tp->sec;
    pmLocaltime(&now, &tmp);
    fprintf(f, "%02d:%02d:%02d.%03d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)(tp->nsec/1000000));
}

static void
print_stamp(FILE *f, __pmTimestamp *stamp)
{
    if (dayflag) {
	char	*ddmm;
	char	*yr;
	char	timebuf[32];	/* for pmCtime result + .xxx */
	time_t	time;

	time = stamp->sec;
	ddmm = pmCtime(&time, timebuf);
	ddmm[10] = ' ';
	ddmm[11] = '\0';
	yr = &ddmm[20];
	fprintf(f, "%s", ddmm);
	print_stamp_msec(f, stamp);
	fprintf(f, " %4.4s", yr);
    }
    else
	print_stamp_msec(f, stamp);
}

static double
unwrap(double current, __pmTimestamp *curtime, checkData *checkdata, instData *idp)
{
    double	outval = current;
    int		wrapflag = 0;
    char	*str = NULL;

    if ((current - idp->lastval) < 0.0 && idp->lasttime.sec > 0) {
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
	if (pmNameInDomArchive(checkdata->desc.indom, idp->inst, &str) < 0)
	    fprintf(stderr, ": %s wrap", typeStr(checkdata->desc.type));
	else {
	    fprintf(stderr, "[%s]: %s wrap", str, typeStr(checkdata->desc.type));
	    free(str);
	}
	fprintf(stderr, "\n\tvalue %.0f at ", idp->lastval);
	print_stamp(stderr, &idp->lasttime);
	fprintf(stderr, "\n\tvalue %.0f at ", current);
	print_stamp(stderr, curtime);
	if (vflag)
	    fprintf(stderr, "\n\tdifference %.0f", current - idp->lastval);
	fputc('\n', stderr);
    }

    return outval;
}

static void
newHashInst(pmValue *vp,
	checkData *checkdata,		/* updated by this function */
	int valfmt,
	__pmTimestamp *timestamp)	/* timestamp for this sample */
{
    int		sts;
    size_t	size;
    pmAtomValue av;
    instData	*idp;

    if ((sts = pmExtractValue(valfmt, vp, checkdata->desc.type, &av, PM_TYPE_DOUBLE)) < 0) {
	fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
	print_stamp(stderr, timestamp);
	fprintf(stderr, "] ");
	print_metric(stderr, checkdata->desc.pmid);
	fprintf(stderr, ": pmExtractValue failed: %s\n", pmErrStr(sts));
	fprintf(stderr, "%s: possibly corrupt archive?\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }
    size = sizeof(instData);
    if ((idp = (instData *)malloc(size)) == NULL) {
	pmNoMem("newHashInst: instData", size, PM_FATAL_ERR);
    }
    idp->inst = vp->inst;
    idp->lastval = av.d;
    idp->lasttime = *timestamp;
    if ((sts = __pmHashAdd(vp->inst, (void *)idp, &checkdata->insthash)) < 0) {
	fprintf(stderr, "newHashInst: __pmHashAdd(%d, ...) for pmID %s failed: %s\n",
	    vp->inst, pmIDStr(checkdata->desc.pmid), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
    if (pmDebugOptions.appl1) {
	char	*name;

	fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
	print_stamp(stderr, timestamp);
	fprintf(stderr, "] ");
	print_metric(stderr, checkdata->desc.pmid);
	if (vp->inst == PM_INDOM_NULL)
	    fprintf(stderr, ": new singular metric\n");
	else {
	    fprintf(stderr, ": new metric-instance pair ");
	    if (pmNameInDomArchive(checkdata->desc.indom, vp->inst, &name) < 0)
		fprintf(stderr, "%d\n", vp->inst);
	    else {
		fprintf(stderr, "\"%s\"\n", name);
		free(name);
	    }
	}

    }
}

static void
newHashItem(pmValueSet *vsp,
	pmDesc *desc,
	checkData *checkdata,		/* output from this function */
	__pmTimestamp *timestamp)	/* timestamp for this sample */
{
    int j;
    int	sts;

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

    __pmHashInit(&checkdata->insthash);
    if ((sts = __pmHashPreAlloc(vsp->numval, &checkdata->insthash)) < 0) {
	fprintf(stderr, "newHashItem: __pmHashPreAlloc(%d, ...) for pmID %s failed: %s\n",
	    vsp->numval, pmIDStr(checkdata->desc.pmid), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
    for (j = 0; j < vsp->numval; j++) {
	newHashInst(&vsp->vlist[j], checkdata, vsp->valfmt, timestamp);
    }
}

static double
timestampToReal(const __pmTimestamp *val)
{
    return val->sec + ((long double)val->nsec / (long double)1000000000);
}

static void
docheck(__pmResult *result)
{
    int			i, j;
    int			sts;
    pmDesc		desc;
    pmAtomValue 	av;
    pmValue		*vp;
    pmValueSet		*vsp;
    __pmHashNode	*hptr = NULL;
    checkData		*checkdata = NULL;
    double		diff;
    __pmTimestamp	timediff;

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];

	if (pmDebugOptions.appl1) {
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
		/*
		 * add to hashlist to suppress repeated error messages
		 * ... but of course no checks on result values that depend
		 * on the pmDesc are possible
		 */
		if (__pmHashAdd(vsp->pmid, NULL, &hashlist) < 0) {
		    fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
		    print_stamp(stderr, &result->timestamp);
		    fprintf(stderr, "] ");
		    print_metric(stderr, vsp->pmid);
		    fprintf(stderr, ": __pmHashAdd bad failed (internal pmlogcheck error)\n");
		}
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
	    if (__pmHashAdd(vsp->pmid, (void*)checkdata, &hashlist) < 0) {
		fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
		print_stamp(stderr, &result->timestamp);
		fprintf(stderr, "] ");
		print_metric(stderr, vsp->pmid);
		fprintf(stderr, ": __pmHashAdd good failed (internal pmlogcheck error)\n");
		continue;
	    }
	}
	else if (hptr->data != NULL) {
	    /* pmid previously looked up - update statistics */
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
		__pmHashNode	*hnp;
		instData	*idp = NULL;

		vp = &vsp->vlist[j];
		hnp = __pmHashSearch(vp->inst, &checkdata->insthash);
		if (hnp == NULL) {
		    /* first time for this inst and this metric */
		    newHashInst(vp, checkdata, vsp->valfmt, &result->timestamp);
		    continue;
		}
		idp = (instData *)hnp->data;

		timediff = result->timestamp;
		__pmTimestampDec(&timediff, &idp->lasttime);
		if (timediff.sec < 0 || timediff.nsec < 0) {
		    /* clip negative values at zero */
		    timediff.sec = 0;
		    timediff.nsec = 0;
		}
		diff = timestampToReal(&timediff);
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
		    if (pmDebugOptions.appl2) {
			fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
			print_stamp(stderr, &result->timestamp);
			fprintf(stderr, "] ");
			print_metric(stderr, checkdata->desc.pmid);
			fprintf(stderr, ": current counter value is %.0f\n", av.d);
		    }
		    if (nowrap == 0)
			unwrap(av.d, &result->timestamp, checkdata, idp);
		}
		idp->lastval = av.d;
		idp->lasttime = result->timestamp;
	    }
	}
    }
}

int
pass3(__pmContext *ctxp, char *archname, pmOptions *opts)
{
    struct timeval	timespan;
    int			sts;
    __pmResult		*result;
    __pmTimestamp	label_stamp;
    __pmTimestamp	last_stamp;
    __pmTimestamp	delta_stamp;
    __pmHashNode	*hptr;

    l_ctxp = ctxp;
    l_archname = archname;
    label_stamp = goldenstart;

    if (vflag)
	fprintf(stderr, "%s: start pass3\n", archname);

    /* check which timestamp print format we should be using */
    timespan = opts->finish;
    tsub(&timespan, &opts->start);
    if (timespan.tv_sec > 86400) /* seconds per day: 60*60*24 */
	dayflag = 1;

    if (opts->start_optarg == NULL && opts->origin_optarg == NULL && 
	opts->align_optarg == NULL) {
	/*
	 * No -S or -O or -A ... start from the epoch in case there are
	 * records with a timestamp _before_ the label timestamp.
	 */
	opts->start.tv_sec = opts->start.tv_usec = 0;
    }

    if ((sts = pmSetMode(PM_MODE_FORW, &opts->start, 0)) < 0) {
	fprintf(stderr, "%s: pmSetMode failed: %s\n", l_archname, pmErrStr(sts));
	return STS_FATAL;
    }

    sts = 0;
    last_stamp.sec = opts->start.tv_sec;
    last_stamp.nsec = opts->start.tv_usec * 1000;
    for ( ; ; ) {
	/*
	 * we need the next record with no fancy checks or record
	 * skipping in libpcp, so use __pmLogRead_ctx() in preference
	 * to pmFetchArchive()
	 */
	sts = __pmLogRead_ctx(l_ctxp, l_ctxp->c_mode, NULL, &result, PMLOGREAD_NEXT);
	if (sts < 0)
	    break;
	result_count++;
	delta_stamp = result->timestamp;
	__pmTimestampDec(&delta_stamp, &label_stamp);
	if (delta_stamp.sec < 0 || delta_stamp.nsec < 0) {
	    fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
	    print_stamp(stderr, &result->timestamp);
	    fprintf(stderr, "]: timestamp before label timestamp: ");
	    print_stamp(stderr, &label_stamp);
	    fprintf(stderr, "\n");
	}
	delta_stamp = result->timestamp;
	__pmTimestampDec(&delta_stamp, &last_stamp);
	if (pmDebugOptions.appl0) {
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
	    fprintf(stderr, "] delta(stamp)=%.3fsec", timestampToReal(&delta_stamp));
	    fprintf(stderr, " numpmid=%d sum(numval)=%d", result->numpmid, sum_val);
	    if (cnt_noval > 0)
		fprintf(stderr, " count(numval=0)=%d", cnt_noval);
	    if (cnt_err > 0)
		fprintf(stderr, " count(numval<0)=%d", cnt_err);
	    fputc('\n', stderr);
	}
	if (delta_stamp.sec < 0 || delta_stamp.nsec < 0) {
	    /* time went backwards! */
	    fprintf(stderr, "%s.%d:[", l_archname, l_ctxp->c_archctl->ac_vol);
	    print_stamp(stderr, &result->timestamp);
	    fprintf(stderr, "]: timestamp went backwards, prior timestamp: ");
	    print_stamp(stderr, &last_stamp);
	    fprintf(stderr, "\n");
	}

	last_stamp = result->timestamp;
	if ((opts->finish.tv_sec > result->timestamp.sec) ||
	    ((opts->finish.tv_sec == result->timestamp.sec) &&
	     (opts->finish.tv_usec >= result->timestamp.nsec / 1000))) {
	    if (result->numpmid == 0) {
		/*
		 * MARK record ... make sure wrap check is not done
		 * at next fetch (mimic interp.c from libpcp)
		 */
		/* walk hash list of metrics */
		for (hptr = __pmHashWalk(&hashlist, PM_HASH_WALK_START);
		     hptr != NULL;
		     hptr = __pmHashWalk(&hashlist, PM_HASH_WALK_NEXT)) {
		    checkData		*checkdata;
		    __pmHashNode	*hnp;
		    checkdata = (checkData *)hptr->data;
		    for (hnp = __pmHashWalk(&checkdata->insthash, PM_HASH_WALK_START);
			 hnp != NULL;
			 hnp = __pmHashWalk(&checkdata->insthash, PM_HASH_WALK_NEXT)) {
			((instData *)(hnp->data))->lasttime.sec = 0;
		    }
		}

		mark_count++;
	    }
	    else
		docheck(result);
	    __pmFreeResult(result);
	}
	else {
	    __pmFreeResult(result);
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

    /*
     * free all hash tables
     */
    for (hptr = __pmHashWalk(&hashlist, PM_HASH_WALK_START);
	 hptr != NULL;
	 hptr = __pmHashWalk(&hashlist, PM_HASH_WALK_NEXT)) {
	if (hptr->data != NULL) {
	    checkData		*checkdata;
	    __pmHashNode	*hnp;
	    checkdata = (checkData *)hptr->data;
	    for (hnp = __pmHashWalk(&checkdata->insthash, PM_HASH_WALK_START);
		 hnp != NULL;
		 hnp = __pmHashWalk(&checkdata->insthash, PM_HASH_WALK_NEXT)) {
		free(hnp->data);
	    }
	    __pmHashFree(&checkdata->insthash);
	    free(hptr->data);
	}
    }
    __pmHashFree(&hashlist);

    return STS_OK;
}
