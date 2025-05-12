/*
 * pmval - simple performance metrics value dumper
 *
 * Copyright (c) 2014-2015,2022 Red Hat.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "pmval.h"

static pmLongOptions longopts[] = {
    PMAPI_GENERAL_OPTIONS,
    PMOPT_SPECLOCAL,
    PMOPT_LOCALPMDA,
    PMOPT_CONTAINER,
    PMOPT_DERIVED,
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "delay", 0, 'd', 0, "delay, pause between updates for archive replay" },
    { "precision", 1, 'f', "N", "fixed output format with N digits precision" },
    { "instances", 1, 'i', "INST", "comma-separated metrics instance list" },
    { "raw", 0, 'r', 0, "output raw counter values (no rate conversion)" },
    { "nointerp", 1, 'U', "FILE", "non-interpolated fetching; ignores interval" },
    { "verbose", 0, 'v', 0, "increase diagnostic output" },
    { "width", 1, 'w', "N", "set the width of each column of output" },
    { "filter", 1, 'x', "VALUE", "store to the metric before fetching (filter)" },
    { "timestamp", 0, 'X', 0, "include date and microseconds in reported timestamps" },
    PMAPI_OPTIONS_END
};

static int override(int, pmOptions *);
static pmOptions opts = {
    .flags = PM_OPTFLAG_DONE | PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
    .short_options = PMAPI_OPTIONS "df:i:K:LrU:vw:x:X",
    .long_options = longopts,
    .short_usage = "[options] metricname",
    .override = override,
};

int			verbose;
int			archive;

static int		pauseFlag;
static int		rawEvents;
static int		rawCounter;
static int		rawArchive;
static int		fixed = -1;
static int		nosamples;
static int		cols;		/* width of output column(s) */
static int		havePrev;	/* have one sample, can compute rate */
static int		amode = PM_MODE_INTERP;		/* archive scan mode */
static char *		source;
static char *		tzlabel;
static pmTime *		pmtime;
static pmTimeControls	controls;
static Context		context;
static int		reporting;	/* set once reporting values starts */
static int		Xflag;

/*
 * Compare two InstPair's on their id fields.
 * This function is passed as an argument to qsort, hence the ugly casts.
 */
static int	/* -1 less, 0 equal, 1 greater */
compare(const void *pair1, const void *pair2)
{
    if (((InstPair *)pair1)->id < ((InstPair *)pair2)->id) return -1;
    if (((InstPair *)pair1)->id > ((InstPair *)pair2)->id) return 1;
    return 0;
}

/* Does the Context have names for all instances in the pmValueSet? */
static int		/* 1 yes, 0 no */
chkinsts(Context *x, pmValueSet *vs)
{
    int      i, j;

    if (x->desc.indom == PM_INDOM_NULL || rawEvents)
	return 1;

    for (i = 0; i < vs->numval; i++) {
	for (j = 0; j < x->inum; j++) {
	    if (vs->vlist[i].inst == x->ipairs[j].id)
		break;
	}
	if (j == x->inum)
	    return 0;
    }
    return 1;
}

/*
 * Fill in current instances into given Context.
 * Instances sorted by instance identifier.
 */
static void
initinsts(Context *x)
{
    int      *ip;
    char     **np;
    InstPair *pp;
    int      n;
    int      e;
    int      i;

    if (x->desc.indom == PM_INDOM_NULL)
	x->inum = 0;
    else {

	/* fill in instance ids for given profile */
	if (! x->iall) {
	    n = x->inum;
	    np = x->inames;
	    ip = (int *)malloc(n * sizeof(int));
	    if (ip == NULL) {
		pmNoMem("pmval.ip", n * sizeof(int), PM_FATAL_ERR);
	    }
	    x->iids = ip;
	    for (i = 0; i < n; i++) {
		if (archive)
		    e = pmLookupInDomArchive(x->desc.indom, *np);
		else
		    e = pmLookupInDom(x->desc.indom, *np);
		if (e < 0) {
            	    fprintf(stderr, "%s: instance %s not available\n",
			    pmGetProgname(), *np);
            	    exit(EXIT_FAILURE);
		}
		*ip = e;
		np++;  ip++;
	    }
	    ip = x->iids;
	    np = x->inames;
	    if ((e = pmAddProfile(x->desc.indom, x->inum, x->iids)) < 0) {
		fprintf(stderr, "%s: pmAddProfile: %s\n",
			pmGetProgname(), pmErrStr(e));
		exit(EXIT_FAILURE);
	    }
	}

	/* find all available instances */
	else {
	    if (archive)
		n = pmGetInDomArchive(x->desc.indom, &ip, &np);
	    else
		n = pmGetInDom(x->desc.indom, &ip, &np);
	    if (n < 0) {
                fprintf(stderr, "%s: pmGetInDom(%s): %s\n",
			pmGetProgname(), pmInDomStr(x->desc.indom), pmErrStr(n));
                exit(EXIT_FAILURE);
	    }
            x->inum = n;
	    x->iids = ip;
	    x->inames = np;
	}

	/* build InstPair list and sort */
	pp = (InstPair *)malloc(n * sizeof(InstPair));
	if (pp == NULL) {
	    pmNoMem("pmval.pp", n * sizeof(InstPair), PM_FATAL_ERR);
	}
	x->ipairs = pp;
	for (i = 0; i < n; i++) {
	    pp->id = *ip;
	    pp->name = *np;
	    ip++;  np++; pp++;
	}
	qsort(x->ipairs, (size_t)n, sizeof(InstPair), compare);
    }
}

/* Initialize API and fill in internal description for given Context. */
static void
initapi(Context *x, pmMetricSpec *msp, int argc, char **argv)
{
    const char *name;
    int e;

    x->metric = msp->metric;
    if (msp->ninst > 0) {
	x->inum = msp->ninst;
	x->iall = (x->inum == 0);
	x->inames = &msp->inst[0];
    }
    x->handle = pmWhichContext();

    name = (const char *)x->metric;
    if ((e = pmLookupName(1, &name, &(x->pmid))) < 0) {
	fprintf(stderr, "%s: pmLookupName(%s): %s\n", pmGetProgname(),
		name, pmErrStr(e));
	exit(EXIT_FAILURE);
    }

    if ((e = pmLookupDesc(x->pmid, &(x->desc))) < 0) {
	fprintf(stderr, "%s: pmLookupDesc: %s\n", pmGetProgname(), pmErrStr(e));
	exit(EXIT_FAILURE);
    }

    if (x->desc.indom == PM_INDOM_NULL && msp->ninst > 0) {
	fprintf(stderr, "%s: %s: singular metrics do not have instances\n",
		pmGetProgname(), msp->metric);
	exit(EXIT_FAILURE);
    }

    if (x->desc.type == PM_TYPE_EVENT ||
	x->desc.type == PM_TYPE_HIGHRES_EVENT) {
	amode = PM_MODE_FORW;	/* do no interpolate events */
	rawArchive = archive;
	rawEvents = 1;
    }

    if (x->desc.sem == PM_SEM_COUNTER) {
	if (x->desc.units.dimTime == 0)
	    x->scale = 1.0;
	else {
	    if (x->desc.units.scaleTime > PM_TIME_SEC)
		x->scale = pow(60, (PM_TIME_SEC - x->desc.units.scaleTime));
	    else
		x->scale = pow(1000, (PM_TIME_SEC - x->desc.units.scaleTime));
	}
    }
}

/* print the timestamp (various precisions) for archives */
static void
mytimestamp(struct timespec *stamp)
{
    time_t		sec = stamp->tv_sec;
    struct tm		tmp;

    pmLocaltime(&sec, &tmp);
    if (!Xflag) {
	printf("%02d:%02d:%02d.%03d",
			tmp.tm_hour, tmp.tm_min, tmp.tm_sec,
			(int)stamp->tv_nsec / 1000000);
    } else {
	char		timebuf[32];	/* for pmCtime result + .xxx */
	char	       *ddmm;
	char	       *yr;

	ddmm = pmCtime(&sec, timebuf);
	ddmm[10] = '\0';
	yr = &ddmm[20];
	if (Xflag == 1)
	    printf("%s %02d:%02d:%02d.%06d %4.4s", ddmm,
			tmp.tm_hour, tmp.tm_min, tmp.tm_sec,
			(int)stamp->tv_nsec / 1000, yr);
	else /* -XX highest resolution - nanoseconds */
	    printf("%s %02d:%02d:%02d.%09d %4.4s", ddmm,
			tmp.tm_hour, tmp.tm_min, tmp.tm_sec,
			(int)stamp->tv_nsec, yr);
    }
}

/* Fetch metric values. */
static int
getvals(Context *x,		/* in - full pm description */
        pmHighResResult **vs)		/* alloc - pm values */
{
    pmHighResResult	*r;
    int			e;
    int			i;

    if (rawArchive) {
	/*
	 * for -U mode, read until we find either a pmHighResResult with the
	 * pmid we are after, or a mark record
	 */
	for ( ; ; ) {
	    e = pmFetchHighResArchive(&r);
	    if (e < 0)
		break;

	    if (r->numpmid == 0) {
		if (opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE)
		    mytimestamp(&r->timestamp);
		printf("  Archive logging suspended\n");
		reporting = 0;
		pmFreeHighResResult(r);
		return -1;
	    }

	    for (i = 0; i < r->numpmid; i++) {
		if (r->vset[i]->pmid == x->pmid)
		    break;
	    }
	    if (i != r->numpmid)
		break;
	    pmFreeHighResResult(r);
	}
    }
    else {
	e = pmFetchHighRes(1, &(x->pmid), &r);
	i = 0;
    }

    if (e < 0) {
	if (e == PM_ERR_EOL) {
	    if (opts.guiflag) {
		pmTimeStateBounds(&controls, pmtime);
		return -1;
	    }
	    exit(EXIT_SUCCESS);
	}
	if (rawArchive)
	    fprintf(stderr, "\n%s: pmFetchArchive: %s\n", pmGetProgname(), pmErrStr(e));
	else
	    fprintf(stderr, "\n%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(e));
	exit(EXIT_FAILURE);
    }

    if (opts.guiflag)
	pmTimeStateAck(&controls, pmtime);

    if (pmtimespecToReal(&r->timestamp) > pmtimespecToReal(&opts.finish)) {
	pmFreeHighResResult(r);
	return -2;
    }

    e = r->vset[i]->numval;
    if (e == 0) {
	if (opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE) {
	    mytimestamp(&r->timestamp);
	    printf("  ");
	}
	if (!rawEvents)
	    printf("No values available\n");
	else if (rawEvents && verbose)
	    printf("%s: No values available\n", x->metric);
	pmFreeHighResResult(r);
	return -1;
    }
    else if (e < 0) {
	if (rawEvents && e == PM_ERR_NOTCONN) {
	    pmFreeHighResResult(r);
	    exit(EXIT_SUCCESS);
	}
	else if (rawArchive) {
	    fprintf(stderr, "\n%s: pmFetchArchive: %s\n",
			pmGetProgname(), pmErrStr(r->vset[i]->numval));
	} else {
	    fprintf(stderr, "\n%s: pmFetch: %s\n",
			pmGetProgname(), pmErrStr(r->vset[i]->numval));
	}
	pmFreeHighResResult(r);
	return -1;
    }

    *vs = r;
    qsort(r->vset[i]->vlist,
          (size_t)r->vset[i]->numval,
          sizeof(pmValue),
          compare);

    return i;
}

static void
timestep(struct timeval delta)
{
    /* time moved, may need to wait for previous value again */
    havePrev = 0;
}


/* How many print positions required for value of given type? */
static int
howide(int type)
{
    switch (type) {
    case PM_TYPE_32: return 11;
    case PM_TYPE_U32: return 11;
    case PM_TYPE_64: return 21;
    case PM_TYPE_U64: return 21;
    case PM_TYPE_FLOAT: return 13;
    case PM_TYPE_DOUBLE: return 21;
    case PM_TYPE_STRING: return 21;
    case PM_TYPE_HIGHRES_EVENT:
    case PM_TYPE_EVENT: return 0;
    case PM_TYPE_AGGREGATE_STATIC:
    case PM_TYPE_AGGREGATE: return 21;
    default:
	fprintf(stderr, "pmval: unknown performance metric value type %s\n", pmTypeStr(type));
	exit(EXIT_FAILURE);
    }
}

/* Print parameter values as output header. */
static void
printhdr(Context *x)
{
    pmUnits		units;
    char		tbfr[26];
    const char		*u;

    if (!rawEvents)
	printf("metric:    %s\n", x->metric);

    if (opts.context != PM_CONTEXT_ARCHIVE)
	printf("host:      %s\n", x->hostname);
    else {
	time_t		time;
	printf("archive:   %s\n", source);
	printf("host:      %s\n", x->hostname);
	time = opts.origin.tv_sec;
	printf("start:     %s", pmCtime(&time, tbfr));
	if (opts.finish.tv_sec != PM_MAX_TIME_T) {
	    time = opts.finish.tv_sec;
	    printf("end:       %s", pmCtime(&time, tbfr));
	}
    }

    if (rawEvents)
	goto footer;

    printf("semantics: ");
    switch (x->desc.sem) {
    case PM_SEM_COUNTER:
	printf("cumulative counter");
	if (! rawCounter) printf(" (converting to rate)");
	break;
    case PM_SEM_INSTANT:
	printf("instantaneous value");
	break;
    case PM_SEM_DISCRETE:
	printf("discrete instantaneous value");
	break;
    default:
	printf("unknown");
    }
    putchar('\n');

    units = x->desc.units;
    u = pmUnitsStr(&units);
    printf("units:     %s", *u == '\0' ? "none" : u);
    if ((! rawCounter) && (x->desc.sem == PM_SEM_COUNTER)) {
	printf(" (converting to ");
	if (units.dimTime == 0) units.scaleTime = PM_TIME_SEC;
	units.dimTime--;
	if ((units.dimSpace == 0) && (units.dimTime == 0) && (units.dimCount == 0))
	    printf("time utilization)");
	else {
	    u = pmUnitsStr(&units);
	    printf("%s)", *u == '\0' ? "none" : u);
	}
    }
    putchar('\n');

footer:
    /* sample count and interval */
    if (opts.samples < 0) printf("samples:   all\n");
    else printf("samples:   %d\n", opts.samples);
    if ((opts.samples > 1) &&
	(opts.context != PM_CONTEXT_ARCHIVE || amode == PM_MODE_INTERP))
	printf("interval:  %1.2f sec\n", pmtimespecToReal(&opts.interval));
}

/* Print instance identifier names as column labels. */
static void
printlabels(Context *x)
{
    int		n = x->inum;
    InstPair	*pairs = x->ipairs;
    int		i;
    static int	style = -1;

    if (rawEvents)
	return;

    if (style == -1) {
	InstPair	*ip = pairs;
	style = 0;
	for (i = 0; i < n; i++) {
	    if (strlen(ip->name) > cols) {
		style = 2;		/* too wide */
		break;
	    }
	    if (strlen(ip->name) > cols-3)
		style = 1;		/* wide enough to change shift */
	    ip++;
	}
	if (style == 2) {
	    ip = pairs;
	    for (i = 0; i < n; i++) {
		printf("full label for instance[%d]: %s\n", i, ip->name);
		ip++;
	    }
	}
    }

    putchar('\n');
    for (i = 0; i < n; i++) {
	if ((opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE) && i == 0) {
	    if (Xflag > 1)
		printf("%*c", 34, ' ');
	    else if (Xflag == 1)
		printf("%*c", 31, ' ');
	    else
		printf("%*c", 12, ' ');
	}
	if (rawCounter || (x->desc.sem != PM_SEM_COUNTER) || style != 0)
	    printf("%*.*s ", cols, cols, pairs->name);
	else {
	    if (fixed == -1) {
		/* shift left by 3 places for decimal points in rate */
		printf("%*.*s    ", cols-3, cols-3, pairs->name);
	    }
	    else {
		/* no shift for fixed format */
		printf("%*.*s ", cols, cols, pairs->name);
	    }
	}
	pairs++;
    }
    if (n > 0) putchar('\n');
}

void
printreal(double v, int sem, int minwidth)
{
    char	*fmt;

    /*
     *   <--  minwidth -->
     *   xxxxxxxxxxxxxxxxx
     *                   !	no value
     *           x.xxxE-xx	< 0.1
     *              0.0___	0
     *              x.xxxx	0.1 ... 0.9999
     *              x.xxx_	1 ... 9.999
     *		   xx.xx__	10 ... 99.99
     *            xxx.x___	100 ... 999.9
     *           xxxx.____	1000 ... 9999
     *           x.xxxE+xx	> 9999
     */

    if (v < 0.0 && sem == PM_SEM_COUNTER)
	printf("%*s", minwidth, "!");
    else {
	if (fixed != -1) {
	    printf("%*.*f", minwidth, fixed, v);
	}
	else {
	    if (v == 0) {
		fmt = "%*.0f.0   ";
		minwidth -= 5;
	    }
	    else if (v < 0.1 || v > 9999)
		fmt = "%*.3E";
	    else if (v <= 0.9999)
		fmt = "%*.4f";
	    else if (v <= 9.999) {
		fmt = "%*.3f ";
		minwidth -= 1;
	    }
	    else if (v <= 99.99) {
		fmt = "%*.2f  ";
		minwidth -= 2;
	    }
	    else if (v <= 999.9) {
		fmt = "%*.1f   ";
		minwidth -= 3;
	    }
	    else {
		fmt = "%*.0f.    ";
		minwidth -= 5;
	    }
	    printf(fmt, minwidth, v);
	}
    }
}

/* Print performance metric values */
static void
printvals(Context *x, pmValueSet *vset)
{
    int 	i, j;
    pmAtomValue	av;
    int		doreal = 0;

    if (x->desc.type == PM_TYPE_FLOAT || x->desc.type == PM_TYPE_DOUBLE)
	doreal = 1;

    /* null instance domain */
    if (x->desc.indom == PM_INDOM_NULL) {
	if (vset->numval == 1) {
	    if (doreal) {
		int	sts;
		sts = pmExtractValue(vset->valfmt, &vset->vlist[0], x->desc.type, &av, PM_TYPE_DOUBLE);
		if (sts < 0) {
		    fprintf(stderr, "%s:printvals pmExtractValue: %s\n", pmGetProgname(), pmErrStr(sts));
		    exit(EXIT_FAILURE);
		}
		printreal(av.d, x->desc.sem, cols);
	    }
	    else
		pmPrintValue(stdout, vset->valfmt, x->desc.type, &vset->vlist[0], cols);

	}
	else
	    printf("%*s", cols, "?");
	putchar('\n');
    }

    /* non-null instance domain */
    else {
	for (i = 0; i < x->inum; i++) {
	    for (j = 0; j < vset->numval; j++) {
		if (vset->vlist[j].inst == x->ipairs[i].id)
		    break;
	    }
	    if (j < vset->numval) {
		if (doreal) {
		    int		sts;
		    sts = pmExtractValue(vset->valfmt, &vset->vlist[j], x->desc.type, &av, PM_TYPE_DOUBLE);
		    if (sts < 0) {
			fprintf(stderr, "%s:printvals[%d] pmExtractValue: %s\n", pmGetProgname(), j, pmErrStr(sts));
			exit(EXIT_FAILURE);
		    }
		    printreal(av.d, x->desc.sem, cols);
		}
		else
		    pmPrintValue(stdout, vset->valfmt, x->desc.type, &vset->vlist[j], cols);
	    }
	    else
		printf("%*s", cols, "?");
	    putchar(' ');
	}
	putchar('\n');

	for (j = 0; j < vset->numval; j++) {
	    for (i = 0; i < x->inum; i++) {
		if (vset->vlist[j].inst == x->ipairs[i].id)
		    break;
	    }
	    if (x->iall == 1 && i == x->inum) {
		printf("Warning: value=");
		if (doreal) {
		    int 	sts;
		    sts = pmExtractValue(vset->valfmt, &vset->vlist[j], x->desc.type, &av, PM_TYPE_DOUBLE);
		    if (sts < 0) {
			fprintf(stderr, "%s:printvals[%d] pmExtractValue: %s\n", pmGetProgname(), j, pmErrStr(sts));
			exit(EXIT_FAILURE);
		    }
		    printreal(av.d, x->desc.sem, 1);
		}
		else
		    pmPrintValue(stdout, vset->valfmt, x->desc.type, &vset->vlist[j], 1);
		printf(", but instance=%d is unknown\n", vset->vlist[j].inst);
	    }
	}
    }
}

/* Print single performance metric rate value */
static void
printrate(int     valfmt,	/* from pmValueSet */
          int     type,		/* from pmDesc */
          pmValue *val1,	/* current value */
          pmValue *val2,	/* previous value */
	  double  delta,	/* time difference between samples */
          int     minwidth)	/* output is at least this wide */
{
    pmAtomValue a, b;
    double	v;
    static int	dowrap = -1;
    int		sts;

    sts = pmExtractValue(valfmt, val1, type, &a, PM_TYPE_DOUBLE);
    if (sts < 0) {
	fprintf(stderr, "%s:printrate prev pmExtractValue: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
    sts = pmExtractValue(valfmt, val2, type, &b, PM_TYPE_DOUBLE);
    if (sts < 0) {
	fprintf(stderr, "%s:printrate this pmExtractValue: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
    v = a.d - b.d;
    if (v < 0.0) {
	if (dowrap == -1) {
	    /* PCP_COUNTER_WRAP in environment enables "counter wrap" logic */
	    if (getenv("PCP_COUNTER_WRAP") == NULL)
		dowrap = 0;
	    else
		dowrap = 1;
	}
	if (dowrap) {
	    switch (type) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    v += (double)UINT_MAX+1;
		    break;
		case PM_TYPE_64:
		case PM_TYPE_U64:
		    v += (double)ULONGLONG_MAX+1;
		    break;
	    }
	}
    }
    v /= delta;
    printreal(v, PM_SEM_COUNTER, minwidth);
}

/* Print performance metric rates */
static void
printrates(Context *x,
	   pmValueSet *vset1, struct timespec stamp1,	/* current values */
	   pmValueSet *vset2, struct timespec stamp2)	/* previous values */
{
    int     i, j, k;
    double  delta;

    /* compute delta from timestamps and convert units */
    delta = x->scale *
	    (pmtimespecToReal(&stamp1) - pmtimespecToReal(&stamp2));

    /* null instance domain */
    if (x->desc.indom == PM_INDOM_NULL) {
	if ((vset1->numval == 1) && (vset2->numval == 1))
	    printrate(vset1->valfmt, x->desc.type, &vset1->vlist[0], &vset2->vlist[0], delta, cols);
	else
	    printf("%*s", cols, "?");
	putchar('\n');
    }

    /* non-null instance domain */
    else {
	for (i = 0; i < x->inum; i++) {
	    for (j = 0; j < vset1->numval; j++) {
		if (vset1->vlist[j].inst != x->ipairs[i].id)
		    continue;
		/* fast path - instance in same position in both valuelists */
		k = j;
		if (k < vset2->numval && vset1->vlist[j].inst == vset2->vlist[k].inst)
		    break;
		/* scan for matching instance identifier, it may have moved */
		for (k = 0; k < vset2->numval; k++)
		    if (vset1->vlist[j].inst == vset2->vlist[k].inst)
			break;
		break;
	    }
	    if ((j < vset1->numval) && (k < vset2->numval) &&
		(vset1->vlist[j].inst == vset2->vlist[k].inst))
		printrate(vset1->valfmt, x->desc.type, &vset1->vlist[j], &vset2->vlist[k], delta, cols);
	    else
		printf("%*s", cols, "?");
	    putchar(' ');
	}
	putchar('\n');

	for (j = 0; j < vset1->numval; j++) {
	    for (i = 0; i < x->inum; i++) {
		if (vset1->vlist[j].inst == x->ipairs[i].id)
		    break;
	    }
	    if (x->iall == 1 && i == x->inum && j < vset2->numval &&
		vset1->vlist[j].inst == vset2->vlist[j].inst) {
		printf("Warning: value=");
		printrate(vset1->valfmt, x->desc.type, &vset1->vlist[j], &vset2->vlist[j], delta, 1);
		printf(", but instance=%d is unknown\n", vset1->vlist[j].inst);
	    }
	}
    }
}

static void
printtime(struct timespec *ts)
{
    char	tbfr[26];
    char	*tp;
    time_t	time;

    time = ts->tv_sec;
    tp = pmCtime(&time, tbfr);
    /*
     * tp -> Ddd Mmm DD HH:MM:SS YYYY\n
     *       0   4   8  1      1 2  2 2
     *                  1      8 0  3 4
     */
    if (rawEvents)
	fprintf(stderr, "[%24.24s]\n", tp);
    else
	fprintf(stderr, "[%8.8s]\n", &tp[11]);
}

/*
 * like isspace, but for a string
 */
static int
hasspace(char *p)
{
    static const char *whitespace = ", \t\n";
    char *set = (char *)whitespace;

    if (p != NULL && *p) {
	while (*set) {
	    if (*p == *set)
		return 1;
	    set++;
	}
    }
    return 0;
}

/*
 * like strtok, but smarter
 */
static char *
getinstance(char *p)
{
    static char	*save;
    char	quot;
    char	*q;
    char	*start;

    if (p == NULL)
	q = save;
    else
	q = p;
    
    while (hasspace(q))
	q++;

    if (*q == '\0')
	return NULL;
    else if (*q == '"' || *q == '\'') {
	quot = *q;
	start = ++q;

	while (*q && *q != quot)
	    q++;
	if (*q == quot)
	    *q++ = '\0';
    }
    else {
	start = q;
	while (*q && !hasspace(q))
	    q++;
    }
    if (*q)
	*q++ = '\0';
    save = q;

    return start;
}

static int
override(int opt, pmOptions *optsp)
{
    /* need to distinguish between zero argument or not requested */
    if (opt == 's') {
	if (atoi(optsp->optarg) == 0)
	    nosamples = 1;
    }
    return 0;	/* continue on with using the common code, always */
}

/*
 * Server-side filtering - send the given filter value to pmcd
 */
static void
initfilters(Context *x, int conntype)
{
    pmAtomValue		atom;
    pmValueSet		*vset;
    pmResult		*result;
    size_t		length;
    int			count, type, sts, i ;

    if (!x->filter || conntype != 0)
	return;

    count = x->inum ? x->inum : 1;
    length = sizeof(pmValueSet) + sizeof(pmValue) * (count - 1);
    if ((vset = (pmValueSet *)calloc(1, length)) == NULL)
	pmNoMem("store vset", length, PM_FATAL_ERR);
    length = sizeof(pmResult);
    if ((result = (pmResult *)calloc(1, length)) == NULL)
	pmNoMem("store result", length, PM_FATAL_ERR);

    result->vset[0] = vset;
    result->numpmid = 1;

    if (rawEvents) {
	type = PM_TYPE_STRING;
	atom.cp = x->filter;
    } else {
	type = x->desc.type;
	sts = __pmStringValue(x->filter, &atom, type);

	if (sts == PM_ERR_TYPE) {
	    fprintf(stderr, "%s: filter \"%s\" incompatible with metric "
			"type (PM_TYPE_%s)\n",
			pmGetProgname(), x->filter, pmTypeStr(type));
	    exit(EXIT_FAILURE);
	}
	if (sts == -ERANGE) {
	    fprintf(stderr, "%s: filter value \"%s\" is out of range for "
			"metric data type (PM_TYPE_%s)\n",
			pmGetProgname(), x->filter, pmTypeStr(type));
	    exit(EXIT_FAILURE);
	}
	if (sts < 0) {
	    fprintf(stderr, "%s: failed to convert value \"%s\": %s\n",
			pmGetProgname(), x->filter, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
    }

    vset->pmid = x->pmid;
    vset->numval = count;

    for (i = 0; i < count; i++) {
	if (x->inum)
	    vset->vlist[i].inst = x->iids[i];
	else
	    vset->vlist[i].inst = PM_IN_NULL;
	if ((sts = __pmStuffValue(&atom, &vset->vlist[i], type)) < 0) {
	    fprintf(stderr, "%s: stuff value \"%s\" failed: %s\n",
			pmGetProgname(), x->filter, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	vset->valfmt = sts;
    }

    if ((sts = pmStore(result)) < 0) {
	fprintf(stderr, "%s: store value \"%s\" failed: %s\n",
			pmGetProgname(), x->filter, pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
    pmFreeResult(result);
}

static inline void
timespec2val(struct timespec *in, struct timeval *out)
{
    out->tv_sec = in->tv_sec;
    out->tv_usec = in->tv_nsec / 1000;
}

int
main(int argc, char *argv[])
{
    int		c;
    int		ctx;
    int		sts;
    int         type;
    int		idx1;
    int		idx2 = 0;		/* initialize to pander to gcc */
    int		forever;
    int		no_values = 0;
    char        *subopt;
    char        *endnum;
    char        *errmsg;
    pmMetricSpec *msp = NULL;
    pmHighResResult *rslt1;		/* current values */
    pmHighResResult *rslt2 = NULL;	/* previous values */

    setlinebuf(stdout);
    context.iall = 1;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'd':
	    pauseFlag = 1;
	    break;

	case 'f':		/* fixed format count */
	    fixed = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || fixed < 0) {
		pmprintf("%s: -f requires +ve numeric argument\n", pmGetProgname());
		opts.errors++;
	    }
	    break;

	case 'i':		/* instance names */
	    context.iall = 0;
	    idx1 = context.inum;
	    subopt = getinstance(opts.optarg);
	    while (subopt != NULL) {
		idx1++;
		context.inames =
		    (char **)realloc(context.inames, idx1 * (sizeof (char *)));
		if (context.inames == NULL)
		    pmNoMem("pmval.ip", idx1 * sizeof(char *), PM_FATAL_ERR);
		*(context.inames + idx1 - 1) = subopt;
		subopt = getinstance(NULL);
	    }
	    context.inum = idx1;
	    break;

	case 'r':
	   rawCounter = 1;
	   break;

	case 'U':		/* non-interpolated archive (undocumented) */
	    __pmAddOptArchive(&opts, opts.optarg);
	    amode = PM_MODE_FORW;
	    rawArchive = 1;
	    break;

	case 'v':
	   verbose = 1;
	   break;

	case 'w':		/* output column width */
	    cols = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || cols < 1) {
		pmprintf("%s: -w requires +ve numeric argument\n", pmGetProgname());
		opts.errors++;
	    }
	    break;

	case 'x':
	    context.filter = opts.optarg;
	    break;

	case 'X':	/* report Ddd Mmm DD <timestamp> YYYY */
	    Xflag++;
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }
    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmUsageMessage(&opts);
	exit(0);
    }
    if (!opts.errors && opts.optind >= argc) {
	pmprintf("%s: error - no metricname specified\n", pmGetProgname());
	opts.errors++;
    }
    else if (!opts.errors && opts.optind < argc - 1) {
	pmprintf("%s: error - too many arguments\n", pmGetProgname());
	opts.errors++;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    /* parse uniform metric spec */
    if (opts.nhosts > 0) {
	source = opts.hosts[0];
	type = 0;
    }
    else if (opts.narchives > 0) {
	source = opts.archives[0];
	type = archive = 1;
    }
    else if (opts.origin_optarg) {
	__pmAddOptArchivePath(&opts);
	source = opts.archives[0];
	type = archive = 1;
    }
    else if (opts.Lflag) {
	source = NULL;
	type = 2;
    }
    else {
	source = "local:";
	type = 0;
    }
    if (pmParseMetricSpec(argv[opts.optind], type, source, &msp, &errmsg) < 0) {
	pmprintf("%s", errmsg);
	free(errmsg);
	opts.errors++;
    }
    else if (msp->isarch == 1) {
	if (opts.narchives == 0)
	    __pmAddOptArchive(&opts, msp->source);
	type = archive = 1; /* may differ now */
	source = msp->source;
    }
    else if (msp->isarch == 2) {
	opts.Lflag = 1;
	source = NULL;
    }
    else {
	if (opts.nhosts == 0)
	    __pmAddOptHost(&opts, msp->source);
	source = msp->source;
    }

    /*
     * As a result of allowing either getopts or the metricspec to specify
     * the source of the metric, we delay option end processing until now.
     */
    opts.flags &= ~PM_OPTFLAG_DONE;
    __pmEndOptions(&opts);

    if (opts.context != PM_CONTEXT_ARCHIVE) {
	if (rawArchive) {
	    pmprintf("%s: uninterpolated mode can only be used with archives",
		    pmGetProgname());
	    opts.errors++;
	}
	if (pauseFlag) {
	    pmprintf("%s: delay can only be used with archives\n", pmGetProgname());
	    opts.errors++;
	}
    }
    else {
	if (opts.guiflag && pauseFlag) {
	    pmprintf("%s: guiflag cannot be used with delay\n", pmGetProgname());
	    opts.errors++;
	}
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if ((sts = ctx = pmNewContext(opts.context, source)) < 0) {
	if (opts.context == PM_CONTEXT_ARCHIVE)
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), source, pmErrStr(sts));
	else if (opts.context == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmGetProgname(), source, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot establish local context: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    if (opts.context == PM_CONTEXT_HOST)
	__pmSetClientIdArgv(argc, argv);

    context.hostname = pmGetContextHostName(ctx);
    if (strlen(context.hostname) == 0) {
	fprintf(stderr, "%s: Cannot evaluate context host name: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    if (pmGetContextOptions(ctx, &opts) < 0) {
	pmflush();	/* runtime errors only at this stage */
	exit(EXIT_FAILURE);
    }
    if (!opts.samples && !nosamples)
	opts.samples = -1;
    if (!opts.guiport)
	opts.guiport = -1;
    if (!opts.finish.tv_sec)
	opts.finish.tv_sec = PM_MAX_TIME_T;
    if (opts.interval.tv_sec == 0 && opts.interval.tv_nsec == 0)
	opts.interval.tv_sec = 1;

    initapi(&context, msp, argc, argv);
    initinsts(&context);
    initfilters(&context, type);

    if (!(opts.guiflag || opts.guiport != -1) &&
	opts.samples < 0 &&
	opts.finish.tv_sec != PM_MAX_TIME_T &&
	amode != PM_MODE_FORW) {
	double start, finish, origin, delta;

	start  = pmtimespecToReal(&opts.start);
	finish = pmtimespecToReal(&opts.finish);
	origin = pmtimespecToReal(&opts.origin);
	delta  = pmtimespecToReal(&opts.interval);

	opts.samples = (int) ((finish - origin) / delta);
	if (opts.samples < 0)
	    opts.samples = 0;	/* if end is before start, no samples thanks */
	else {
	    /*
	     * p stands for posn
	     * + p         + p+delta   + p+2*delta + p+3*delta        + last
	     * |           |           |           |              |   |
	     * +-----------+-----------+-----------+-- ...... ----+---+---> time
	     *             1           2           3              smpls
	     *
	     * So we will perform smpls+1 fetches ... the number of reported
	     * values cannot be determined as it is usually (but not always
	     * thanks to interpolation mode in archives) one less for
	     * PM_SEM_COUNTER metrics.
	     *
	     * samples: as reported in the header output is the number
	     * of fetches to be attempted.
	     */
	    opts.samples++;
	}
	if (pmDebugOptions.appl0) {
	    fprintf(stderr, "first=%.6f", start);
	    printtime(&opts.start);
	    fprintf(stderr, "posn=%.6f", origin);
	    printtime(&opts.origin);
	    fprintf(stderr, "last=%.6f", finish);
	    printtime(&opts.finish);
	    fprintf(stderr, "delta=%.6f samples=%d\n", delta, opts.samples);
	}
    }

    if (opts.timezone)
	tzlabel = opts.timezone;
    else {
	if (!opts.tzflag && !rawEvents)
	    printf("\n");
	tzlabel = (char *)context.hostname;
    }

    if (opts.guiflag || opts.guiport != -1) {
	struct timeval interval, origin, start, finish;

	timespec2val(&opts.interval, &interval);
	timespec2val(&opts.origin, &origin);
	timespec2val(&opts.start, &start);
	timespec2val(&opts.finish, &finish);

	/* set up pmtime control */
	pmWhichZone(&opts.timezone);
	pmtime = pmTimeStateSetup(&controls,
				  opts.context, opts.guiport,
				  interval, origin, start, finish,
				  opts.timezone, tzlabel);
	controls.stepped = timestep;
	opts.guiflag = 1;	/* we're using pmtime control from here on */
    }
    else if (opts.context == PM_CONTEXT_ARCHIVE) { /* no time control, go it alone */
	struct timeval interval, origin;

	timespec2val(&opts.interval, &interval);
	timespec2val(&opts.origin, &origin);
	pmTimeStateMode(amode, interval, &origin);
    }

    forever = (opts.samples < 0 || opts.guiflag);

    if (cols <= 0)
	cols = howide(context.desc.type);

    if ((fixed == 0 && fixed > cols) || (fixed > 0 && fixed > cols - 2)) {
	fprintf(stderr, "%s: -f %d too large for column width %d\n",
		pmGetProgname(), fixed, cols);
	exit(EXIT_FAILURE);
    }

    printhdr(&context);

    /* wait till time for first sample */
    if (opts.context != PM_CONTEXT_ARCHIVE)
	__pmtimespecPause(opts.start);

    /* main loop fetching and printing sample values */
    while (forever || (opts.samples-- > 0)) {
	if (opts.guiflag)
	    pmTimeStateVector(&controls, pmtime);
	if (havePrev == 0) {
	    /*
	     * We don't yet have a value at the previous time point ...
	     * save this value so we can use it to compute the rate if
	     * the metric has counter semantics and we're doing rate
	     * conversion.
	     */
	    if ((idx2 = getvals(&context, &rslt2)) >= 0) {
		/* previous value success */
		havePrev = 1;
		if (context.desc.indom != PM_INDOM_NULL)
		    printlabels(&context);
		if (rawEvents) {
		    if (opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE) {
			mytimestamp(&rslt2->timestamp);
			printf("  ");
		    }
		    printevents(&context, rslt2->vset[idx2], cols);
		    reporting = 1;
		    continue;
		}
		else if (rawCounter || (context.desc.sem != PM_SEM_COUNTER)) {
		    /* not doing rate conversion, report this value immediately */
		    if (opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE)
			mytimestamp(&rslt2->timestamp);
		    printvals(&context, rslt2->vset[idx2]);
		    reporting = 1;
		    continue;
		}
		else if (no_values || reporting) {
		    if (opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE) {
			mytimestamp(&rslt2->timestamp);
			printf("  ");
		    }
		    printf("No values available\n");
		}
		no_values = 0;
		if (opts.guiflag)
		    /* pmtime controls timing */
		    continue;
	    }
	    else if (idx2 == -2)
		/* out the end of the window */
		break;
	    else
		no_values = 1;
	}

	/* wait till time for sample */
	if (!opts.guiflag && (pauseFlag || opts.context != PM_CONTEXT_ARCHIVE))
	    __pmtimespecSleep(opts.interval);

	if (havePrev == 0)
	    continue;	/* keep trying to get the previous sample */

	/* next sample */
	if ((idx1 = getvals(&context, &rslt1)) == -2)
		/* out the end of the window */
		break;
	else if (idx1 < 0) {
	    /*
	     * Fall back to trying to get an initial sample because
	     * although we got the previous sample, we failed to get the
	     * next sample.
	     */
	    havePrev = 0;
	    continue;
	}

	/* refresh instance names */
	if (context.iall && ! chkinsts(&context, rslt1->vset[idx1])) {
	    free(context.iids);
	    if (context.iall)
		free(context.inames);
	    free(context.ipairs);
	    initinsts(&context);
	    printlabels(&context);
	}

	/* print values */
	if (opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE) {
	    mytimestamp(&rslt1->timestamp);
	    if (rawEvents)
		printf("  ");
	}
	if (rawEvents)
	    printevents(&context, rslt1->vset[idx1], cols);
	else if (rawCounter || (context.desc.sem != PM_SEM_COUNTER))
	    printvals(&context, rslt1->vset[idx1]);
	else
	    printrates(&context, rslt1->vset[idx1], rslt1->timestamp,
		       rslt2->vset[idx2], rslt2->timestamp);
	reporting = 1;

	/*
	 * discard previous and save current result, so this value
	 * becomes the previous value at the next iteration
	 */
	pmFreeHighResResult(rslt2);
	rslt2 = rslt1;
	idx2 = idx1;
    }

    /* make valgrind happy */
    if (rslt2 != NULL)
	pmFreeHighResResult(rslt2);
    if (msp != NULL)
	pmFreeMetricSpec(msp);

    pmDestroyContext(ctx);

    /*
     * All serious error conditions have explicit exit() calls, so
     * if we get this far, all has gone well.
     */

    return 0;
}
