/*
 * pmval - simple performance metrics value dumper
 *
 * Copyright (c) 2014-2015 Red Hat.
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
#include "pmapi.h"
#include "impl.h"
#include "pmtime.h"

/* instance id - instance name association */
typedef struct {
    int		id;
    char	*name;
} InstPair;

/* full description of a performance metric */
typedef struct {
    /* external (printable) description */
    const char	*hostname;
    char	*metric;	/* name of metric */
    int		iall;		/* all instances */
    int		inum;		/* number of instances */
    char	**inames;	/* list of instance names */
    /* internal description */
    int		handle;		/* context handle */
    pmID	pmid;		/* metric identifier */
    pmDesc	desc;		/* metric description */
    float	scale;		/* conversion factor for rate */
    int		*iids;		/* list of instance ids */
    /* internal-external association */
    InstPair	*ipairs;	/* sorted array of id-name */
} Context;

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
    { "width", 1, 'w', "N", "set the width of each column of output" },
    PMAPI_OPTIONS_END
};

static int override(int, pmOptions *);
static pmOptions opts = {
    .flags = PM_OPTFLAG_DONE | PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
    .short_options = PMAPI_OPTIONS "df:i:K:LrU:w:",
    .long_options = longopts,
    .short_usage = "[options] metricname",
    .override = override,
};

static int		pauseFlag;
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

/*
 * Processing fetched values
 */

/* Compare two InstPair's on their id fields.
   - This function is passed as an argument to qsort,
     hence the ugly casts. */
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

    if (x->desc.indom == PM_INDOM_NULL)
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


/* Fill in current instances into given Context.
   Instances sorted by instance identifier.  */
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
		__pmNoMem("pmval.ip", n * sizeof(int), PM_FATAL_ERR);
	    }
	    x->iids = ip;
	    for (i = 0; i < n; i++) {
		if (opts.context == PM_CONTEXT_ARCHIVE)
		    e = pmLookupInDomArchive(x->desc.indom, *np);
		else
		    e = pmLookupInDom(x->desc.indom, *np);
		if (e < 0) {
            	    printf("%s: instance %s not available\n", pmProgname, *np);
            	    exit(EXIT_FAILURE);
		}
		*ip = e;
		np++;  ip++;
	    }
	    ip = x->iids;
	    np = x->inames;
	    if ((e = pmAddProfile(x->desc.indom, x->inum, x->iids)) < 0) {
		fprintf(stderr, "%s: pmAddProfile: %s\n", pmProgname, pmErrStr(e));
		exit(EXIT_FAILURE);
	    }
	}

	/* find all available instances */
	else {
	    if (opts.context == PM_CONTEXT_ARCHIVE)
		n = pmGetInDomArchive(x->desc.indom, &ip, &np);
	    else
		n = pmGetInDom(x->desc.indom, &ip, &np);
	    if (n < 0) {
                fprintf(stderr, "%s: pmGetInDom(%s): %s\n", pmProgname, pmInDomStr(x->desc.indom), pmErrStr(n));
                exit(EXIT_FAILURE);
	    }
            x->inum = n;
	    x->iids = ip;
	    x->inames = np;
	}

	/* build InstPair list and sort */
	pp = (InstPair *)malloc(n * sizeof(InstPair));
	if (pp == NULL) {
	    __pmNoMem("pmval.pp", n * sizeof(InstPair), PM_FATAL_ERR);
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
initapi(Context *x)
{
    int e;

    x->handle = pmWhichContext();

    if ((e = pmLookupName(1, &(x->metric), &(x->pmid))) < 0) {
        fprintf(stderr, "%s: pmLookupName(%s): %s\n", pmProgname, x->metric, pmErrStr(e));
        exit(EXIT_FAILURE);
    }

    if ((e = pmLookupDesc(x->pmid, &(x->desc))) < 0) {
        fprintf(stderr, "%s: pmLookupDesc: %s\n", pmProgname, pmErrStr(e));
        exit(EXIT_FAILURE);
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

/* Fetch metric values. */
static int
getvals(Context *x,		/* in - full pm description */
        pmResult **vs)		/* alloc - pm values */
{
    pmResult	*r;
    int		e;
    int		i;

    if (rawArchive) {
	/*
	 * for -U mode, read until we find either a pmResult with the
	 * pmid we are after, or a mark record
	 */
	for ( ; ; ) {
	    e = pmFetchArchive(&r);
	    if (e < 0)
		break;

	    if (r->numpmid == 0) {
		if (opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE)
		    __pmPrintStamp(stdout, &r->timestamp);
		printf("  Archive logging suspended\n");
		pmFreeResult(r);
		return -1;
	    }

	    for (i = 0; i < r->numpmid; i++) {
		if (r->vset[i]->pmid == x->pmid)
		    break;
	    }
	    if (i != r->numpmid)
		break;
	    pmFreeResult(r);
	}
    }
    else {
	e = pmFetch(1, &(x->pmid), &r);
	i = 0;
    }

    if (e < 0) {
	if (e == PM_ERR_EOL && opts.guiflag) {
	    pmTimeStateBounds(&controls, pmtime);
	    return -1;
	}
	if (rawArchive)
	    fprintf(stderr, "\n%s: pmFetchArchive: %s\n", pmProgname, pmErrStr(e));
	else
	    fprintf(stderr, "\n%s: pmFetch: %s\n", pmProgname, pmErrStr(e));
        exit(EXIT_FAILURE);
    }

    if (opts.guiflag)
	pmTimeStateAck(&controls, pmtime);

    if (__pmtimevalToReal(&r->timestamp) > __pmtimevalToReal(&opts.finish)) {
	pmFreeResult(r);
	return -2;
    }

    if (r->vset[i]->numval == 0) {
	if (opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE) {
	    __pmPrintStamp(stdout, &r->timestamp);
	    printf("  ");
	}
	printf("No values available\n");
	pmFreeResult(r);
	return -1;
    }
    else if (r->vset[i]->numval < 0) {
	if (rawArchive)
	    fprintf(stderr, "\n%s: pmFetchArchive: %s\n", pmProgname, pmErrStr(r->vset[i]->numval));
	else
	    fprintf(stderr, "\n%s: pmFetch: %s\n", pmProgname, pmErrStr(r->vset[i]->numval));
	pmFreeResult(r);
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

    printf("metric:    %s\n", x->metric);

    if (opts.context != PM_CONTEXT_ARCHIVE)
	printf("host:      %s\n", x->hostname);
    else {
	printf("archive:   %s\n", source);
	printf("host:      %s\n", x->hostname);
	printf("start:     %s", pmCtime((const time_t *)&opts.origin.tv_sec, tbfr));
	if (opts.finish.tv_sec != INT_MAX)
	    printf("end:       %s", pmCtime((const time_t *)&opts.finish.tv_sec, tbfr));
    }

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

    /* sample count and interval */
    if (opts.samples < 0) printf("samples:   all\n");
    else printf("samples:   %d\n", opts.samples);
    if ((opts.samples > 1) &&
	(opts.context != PM_CONTEXT_ARCHIVE || amode == PM_MODE_INTERP))
	printf("interval:  %1.2f sec\n", __pmtimevalToReal(&opts.interval));
}

/* Print instance identifier names as column labels. */
static void
printlabels(Context *x)
{
    int		n = x->inum;
    InstPair	*pairs = x->ipairs;
    int		i;
    static int	style = -1;

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
	if ((opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE) && i == 0)
	    printf("            ");
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
printreal(double v, int minwidth)
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

    if (fixed != -1) {
	printf("%*.*f", minwidth, fixed, v);
    }
    else {
	if (v < 0.0)
	    printf("%*s", minwidth, "!");
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
printvals(Context *x, pmValueSet *vset, int cols)
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
		pmExtractValue(vset->valfmt, &vset->vlist[0], x->desc.type, &av, PM_TYPE_DOUBLE);
		printreal(av.d, cols);
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
		    pmExtractValue(vset->valfmt, &vset->vlist[j], x->desc.type, &av, PM_TYPE_DOUBLE);
		    printreal(av.d, cols);
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
		    pmExtractValue(vset->valfmt, &vset->vlist[j], x->desc.type, &av, PM_TYPE_DOUBLE);
		    printreal(av.d, 1);
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

    pmExtractValue(valfmt, val1, type, &a, PM_TYPE_DOUBLE);
    pmExtractValue(valfmt, val2, type, &b, PM_TYPE_DOUBLE);
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
    printreal(v, minwidth);
}

/* Print performance metric rates */
static void
printrates(Context *x,
	   pmValueSet *vset1, struct timeval stamp1,	/* current values */
	   pmValueSet *vset2, struct timeval stamp2,	/* previous values */
	   int cols)
{
    int     i, j;
    double  delta;

    /* compute delta from timestamps and convert units */
    delta = x->scale *
	    (__pmtimevalToReal(&stamp1) - __pmtimevalToReal(&stamp2));

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
		if (vset1->vlist[j].inst == x->ipairs[i].id)
		    break;
	    }
	    if ((j < vset1->numval) && (j < vset2->numval) &&
		(vset1->vlist[j].inst == vset2->vlist[j].inst))
		printrate(vset1->valfmt, x->desc.type, &vset1->vlist[j], &vset2->vlist[j], delta, cols);
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
override(int opt, pmOptions *opts)
{
    /* need to distinguish between zero argument or not requested */
    if (opt == 's') {
	if (atoi(opts->optarg) == 0)
	    nosamples = 1;
    }
    return 0;	/* continue on with using the common code, always */
}

int
main(int argc, char *argv[])
{
    int		c;
    int         i;
    int		ctx;
    int		sts;
    int		idx1;
    int		idx2 = 0;		/* initialize to pander to gcc */
    int		forever;
    int		no_values = 0;
    char        *subopt;
    char        *endnum;
    char        *errmsg;
    pmMetricSpec *msp;
    pmResult    *rslt1;		/* current values */
    pmResult    *rslt2;		/* previous values */

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
		pmprintf("%s: -f requires +ve numeric argument\n", pmProgname);
		opts.errors++;
	    }
	    break;

	case 'i':		/* instance names */
	    context.iall = 0;
	    i = context.inum;
	    subopt = getinstance(opts.optarg);
	    while (subopt != NULL) {
		i++;
		context.inames =
		    (char **)realloc(context.inames, i * (sizeof (char *)));
		if (context.inames == NULL)
		    __pmNoMem("pmval.ip", i * sizeof(char *), PM_FATAL_ERR);
		*(context.inames + i - 1) = subopt;
		subopt = getinstance(NULL);
	    }
	    context.inum = i;
	    break;

	case 'r':
	   rawCounter = 1;
	   break;

	case 'U':		/* non-interpolated archive (undocumented) */
	    __pmAddOptArchive(&opts, opts.optarg);
	    amode = PM_MODE_FORW;
	    rawArchive = 1;
	    break;

	case 'w':		/* output column width */
	    cols = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || cols < 1) {
		pmprintf("%s: -w requires +ve numeric argument\n", pmProgname);
		opts.errors++;
	    }
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }

    if (!opts.errors && opts.optind >= argc) {
	pmprintf("Error: no metricname specified\n\n");
	opts.errors++;
    }
    else if (!opts.errors && opts.optind < argc - 1) {
	pmprintf("Error: pmval can only process one metricname at a time\n\n");
	opts.errors++;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    /* parse uniform metric spec */
    if (opts.nhosts > 0) {
	source = opts.hosts[0];
	i = 0;
    }
    else if (opts.narchives > 0) {
	source = opts.archives[0];
	i = 1;
    }
    else if (opts.Lflag) {
	source = NULL;
	i = 2;
    }
    else {
	source = "local:";
	i = 0;
    }
    if (pmParseMetricSpec(argv[opts.optind], i, source, &msp, &errmsg) < 0) {
	pmprintf("%s", errmsg);
	free(errmsg);
	opts.errors++;
    }
    else if (msp->isarch == 1) {
	if (opts.narchives == 0)
	    __pmAddOptArchive(&opts, msp->source);
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
		    pmProgname);
	    opts.errors++;
	}
	if (pauseFlag) {
	    pmprintf("%s: delay can only be used with archives\n", pmProgname);
	    opts.errors++;
	}
    }
    else {
	if (opts.guiflag && pauseFlag) {
	    pmprintf("%s: guiflag cannot be used with delay\n", pmProgname);
	    opts.errors++;
	}
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    context.metric = msp->metric;
    if (msp->ninst > 0) {
	context.inum = msp->ninst;
	context.iall = (context.inum == 0);
	context.inames = &msp->inst[0];
    }

    if ((sts = ctx = pmNewContext(opts.context, source)) < 0) {
	if (opts.context == PM_CONTEXT_ARCHIVE)
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmProgname, source, pmErrStr(sts));
	else if (opts.context == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmProgname, source, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot establish local context: %s\n",
		    pmProgname, pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    context.hostname = pmGetContextHostName(ctx);
    if (strlen(context.hostname) == 0) {
	fprintf(stderr, "%s: Cannot evaluate context host name: %s\n",
		    pmProgname, pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    if (pmGetContextOptions(ctx, &opts) < 0) {
	pmflush();	/* runtime errors only at this stage */
	exit(EXIT_FAILURE);
    }
    if (opts.timezone)
	tzlabel = opts.timezone;
    else {
	if (!opts.tzflag)
	    printf("\n");
	tzlabel = (char *)context.hostname;
    }

    if (!opts.samples && !nosamples)
	opts.samples = -1;
    if (!opts.guiport)
	opts.guiport = -1;
    if (!opts.finish.tv_sec)
	opts.finish.tv_sec = INT_MAX;
    if (opts.interval.tv_sec == 0 && opts.interval.tv_usec == 0)
	opts.interval.tv_sec = 1;

    initapi(&context);
    initinsts(&context);

    if (!(opts.guiflag || opts.guiport != -1) &&
	opts.samples < 0 &&
	opts.finish.tv_sec != INT_MAX &&
	amode != PM_MODE_FORW) {
	double start, finish, origin, delta;

	start  = __pmtimevalToReal(&opts.start);
	finish = __pmtimevalToReal(&opts.finish);
	origin = __pmtimevalToReal(&opts.origin);
	delta  = __pmtimevalToReal(&opts.interval);

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
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    char		tbfr[26];
	    char		*tp;
	    fprintf(stderr, "getargs: first=%.6f", start);
	    tp = pmCtime((const time_t *)&opts.start.tv_sec, tbfr);
	    /*
	     * tp -> Ddd Mmm DD HH:MM:SS YYYY\n
	     *       0   4   8  1      1 2  2 2
	     *                  1      8 0  3 4
	     */
	    fprintf(stderr, "[%8.8s]\n", &tp[11]);
	    fprintf(stderr, "getargs: posn=%.6f", origin);
	    tp = pmCtime((const time_t *)&opts.origin.tv_sec, tbfr);
	    fprintf(stderr, "[%8.8s]\n", &tp[11]);
	    fprintf(stderr, "getargs: last=%.6f", finish);
	    tp = pmCtime((const time_t *)&opts.finish.tv_sec, tbfr);
	    fprintf(stderr, "[%8.8s]\n", &tp[11]);
	    fprintf(stderr, "getargs: delta=%.6f samples=%d\n",
		    delta, opts.samples);
	}
#endif
    }

    if (opts.guiflag || opts.guiport != -1) {
	/* set up pmtime control */
	pmWhichZone(&opts.timezone);
	pmtime = pmTimeStateSetup(&controls, opts.context, opts.guiport,
				  opts.interval, opts.origin, opts.start,
				  opts.finish, opts.timezone, tzlabel);
	controls.stepped = timestep;
	opts.guiflag = 1;	/* we're using pmtime control from here on */
    }
    else if (opts.context == PM_CONTEXT_ARCHIVE) /* no time control, go it alone */
	pmTimeStateMode(amode, opts.interval, &opts.origin);

    /* TODO: bring logic from pmevent, reunify the two binaries */
    if (context.desc.type == PM_TYPE_EVENT ||
	context.desc.type == PM_TYPE_HIGHRES_EVENT) {
	fprintf(stderr, "%s: Cannot display values for event type metrics\n",
		pmProgname);
	exit(EXIT_FAILURE);
    }

    forever = (opts.samples < 0 || opts.guiflag);

    if (cols <= 0)
	cols = howide(context.desc.type);

    if ((fixed == 0 && fixed > cols) || (fixed > 0 && fixed > cols - 2)) {
	fprintf(stderr, "%s: -f %d too large for column width %d\n",
		pmProgname, fixed, cols);
	exit(EXIT_FAILURE);
    }

    printhdr(&context);

    /* wait till time for first sample */
    if (opts.context != PM_CONTEXT_ARCHIVE)
	__pmtimevalPause(opts.start);

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
		if (rawCounter || (context.desc.sem != PM_SEM_COUNTER)) {
		    /* not doing rate conversion, report this value immediately */
		    if (opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE)
			__pmPrintStamp(stdout, &rslt2->timestamp);
		    printvals(&context, rslt2->vset[idx2], cols);
		    continue;
		}
		else if (no_values) {
		    if (opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE) {
			__pmPrintStamp(stdout, &rslt2->timestamp);
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
	    __pmtimevalSleep(opts.interval);

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
	if (opts.guiflag || opts.context == PM_CONTEXT_ARCHIVE)
	    __pmPrintStamp(stdout, &rslt1->timestamp);
	if (rawCounter || (context.desc.sem != PM_SEM_COUNTER))
	    printvals(&context, rslt1->vset[idx1], cols);
	else
	    printrates(&context, rslt1->vset[idx1], rslt1->timestamp,
		       rslt2->vset[idx2], rslt2->timestamp, cols);

	/*
	 * discard previous and save current result, so this value
	 * becomes the previous value at the next iteration
	 */
	pmFreeResult(rslt2);
	rslt2 = rslt1;
	idx2 = idx1;
    }

    /*
     * All serious error conditions have explicit exit() calls, so
     * if we get this far, all has gone well.
     */
    return 0;
}
