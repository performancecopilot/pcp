/*
 * pmval - simple performance metrics value dumper
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
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

/***************************************************************************
 * constants
 ***************************************************************************/

#define ALL_SAMPLES	-1

static char *options = "A:a:D:df:gh:i:K:n:O:p:rs:S:t:T:U:w:zZ:?";
static char usage[] =
    "Usage: %s [options] metricname\n\n"
    "Options:\n"
    "  -A align      align sample times on natural boundaries\n"
    "  -a archive    metrics source is a PCP log archive (interpolate values)\n"
    "  -d            delay, pause between updates for archive replay\n"
    "  -f N          fixed precision output format with N digits to the\n"
    "                right of the decimal point\n"
    "  -g            start in GUI mode with new time control\n"
    "  -h host       metrics source is PMCD on host\n"
    "  -i instance   metric instance or list of instances - elements in an\n"
    "                instance list are separated by commas or whitespace\n"
    "  -K spec       optional additional PMDA spec for local connection\n"
    "                spec is of the form op,domain,dso-path,init-routine\n"
    "  -n pmnsfile   use an alternative PMNS\n"
    "  -O offset     initial offset into the time window\n"
    "  -p port       port number for connection to existing time control\n"
    "  -r            output raw counter values\n"
    "  -S starttime  start of the time window\n"
    "  -s samples    terminate after this many samples\n"
    "  -T endtime    end of the time window\n"
    "  -t interval   sample interval [default 1 second]\n"
    "  -U archive    metrics source is a PCP log archive (do not interpolate\n"
    "                and -t option ignored)\n"
    "  -w width      set the width of each column of output\n"
    "  -Z timezone   set reporting timezone\n"
    "  -z            set reporting timezone to local time of metrics source\n";


/***************************************************************************
 * type definitions
 ***************************************************************************/

/* instance id - instance name association */
typedef struct {
    int  id;
    char *name;
} InstPair;

/* full description of a performance metric */
typedef struct {
    /* external (printable) description */
    char	*host;		/* name of host */
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


/***************************************************************************
 * Globals
 ***************************************************************************/

static char		*archive;
static pmLogLabel	label;
static char		*pmnsfile = PM_NS_DEFAULT;
static char		*rpt_tz;
static char		*rpt_tz_label;
static int		pauseFlag;
static int		raw;
static int		ahtype = PM_CONTEXT_HOST;	/* archive or host? */
		/* have previous sample, so could compute rate if required */
static int		havePrev = 0;
static int		amode = PM_MODE_INTERP;		/* archive scan mode */
static char		local[] = "local:";
static int		gui;
static int		rawarchive;
static int		port = -1;
static pmTime		*pmtime;
static pmTimeControls	controls;
static struct timeval	last = {INT_MAX, 999999};	/* end time for log */
static int		fixed = -1;

/***************************************************************************
 * processing fetched values
 ***************************************************************************/

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

/***************************************************************************
 * interface to performance metrics API
 ***************************************************************************/

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
		if (ahtype == PM_CONTEXT_ARCHIVE)
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
	    if (ahtype == PM_CONTEXT_ARCHIVE)
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

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((e = pmLoadNameSpace(pmnsfile)) < 0) {
	    fprintf(stderr, "%s: pmLoadNameSpace: %s\n", pmProgname, pmErrStr(e));
	    exit(EXIT_FAILURE);
	}
    }

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

    if (rawarchive) {
	/*
	 * for -U mode, read until we find either a pmResult with the
	 * pmid we are after, or a mark record
	 */
	for ( ; ; ) {
	    e = pmFetchArchive(&r);
	    if (e < 0)
		break;

	    if (r->numpmid == 0) {
		if (gui || archive != NULL)
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
	if (e == PM_ERR_EOL && gui) {
	    pmTimeStateBounds(&controls, pmtime);
	    return -1;
	}
	if (rawarchive)
	    fprintf(stderr, "\n%s: pmFetchArchive: %s\n", pmProgname, pmErrStr(e));
	else
	    fprintf(stderr, "\n%s: pmFetch: %s\n", pmProgname, pmErrStr(e));
        exit(EXIT_FAILURE);
    }

    if (gui)
	pmTimeStateAck(&controls, pmtime);

    if ((double)r->timestamp.tv_sec + (double)r->timestamp.tv_usec/1000000 >
	(double)last.tv_sec + (double)last.tv_usec/1000000) {
	pmFreeResult(r);
	return -2;
    }

    if (r->vset[i]->numval == 0) {
	if (gui || archive != NULL) {
	    __pmPrintStamp(stdout, &r->timestamp);
	    printf("  ");
	}
	printf("No values available\n");
	pmFreeResult(r);
	return -1;
    }
    else if (r->vset[i]->numval < 0) {
	if (rawarchive)
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


/***************************************************************************
 * output
 ***************************************************************************/

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
printhdr(Context *x, long smpls, struct timeval delta, struct timeval first)
{
    pmUnits		units;
    char		tbfr[26];
    const char		*u;

    /* metric name */
    printf("metric:    %s\n", x->metric);

    /* live host */
    if (archive == NULL)
	printf("host:      %s\n", pmGetContextHostName(x->handle));

    /* archive */
    else {
	printf("archive:   %s\n", archive);
	printf("host:      %s\n", label.ll_hostname);
	printf("start:     %s", pmCtime(&first.tv_sec, tbfr));
	if (last.tv_sec != INT_MAX)
	    printf("end:       %s", pmCtime(&last.tv_sec, tbfr));
    }

    /* semantics */
    printf("semantics: ");
    switch (x->desc.sem) {
    case PM_SEM_COUNTER:
	printf("cumulative counter");
	if (! raw) printf(" (converting to rate)");
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

    /* units */
    units = x->desc.units;
    u = pmUnitsStr(&units);
    printf("units:     %s", *u == '\0' ? "none" : u);
    if ((! raw) && (x->desc.sem == PM_SEM_COUNTER)) {
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
    if (smpls == ALL_SAMPLES) printf("samples:   all\n");
    else printf("samples:   %ld\n", smpls);
    if (smpls != ALL_SAMPLES && smpls > 1 &&
	(ahtype != PM_CONTEXT_ARCHIVE || amode == PM_MODE_INTERP))
	printf("interval:  %1.2f sec\n", __pmtimevalToReal(&delta));
}

/* Print instance identifier names as column labels. */
static void
printlabels(Context *x, int cols)
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
	if ((gui || archive != NULL) && i == 0)
	    printf("            ");
	if (raw || (x->desc.sem != PM_SEM_COUNTER) || style != 0)
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


/* print single performance metric rate value */
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


/***************************************************************************
 * command line processing
 ***************************************************************************/

#define WHITESPACE ", \t\n"

static int
isany(char *p, char *set)
{
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
    
    while (isany(q, WHITESPACE))
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
	while (*q && !isany(q, WHITESPACE))
	    q++;
    }
    if (*q)
	*q++ = '\0';
    save = q;

    return start;
}

/* extract command line arguments - exits on error */
static void
getargs(int		argc,		/* in - command line argument count */
        char		*argv[],	/* in - argument strings */
        Context		*cntxt,		/* out - full pm description */
        struct timeval	*posn,		/* out - first sample time */
        struct timeval	*delta,		/* out - sample interval */
        long		*smpls,		/* out - number of samples */
	int		*cols)		/* out - output column width */
{
    int		c;
    char        *subopt;
    long	d;
    int		errflag = 0;
    int         i;
    int		src = 0;
    char	*host = local;
    int		sts;
    char        *endnum;
    char        *errmsg;

    char	    *Sflag = NULL;		/* argument of -S flag */
    char	    *Tflag = NULL;		/* argument of -T flag */
    char	    *Aflag = NULL;		/* argument of -A flag */
    char	    *Oflag = NULL;		/* argument of -O flag */
    int		    zflag = 0;			/* for -z */
    char 	    *tz = NULL;		/* for -Z timezone */
    int		    tzh;			/* initial timezone handle */
    struct timeval  logStart;
    struct timeval  first;
    pmMetricSpec   *msp;
    char	    *msg;

    /* fill in default values */
    cntxt->iall = 1;
    delta->tv_sec = 1;
    *smpls = ALL_SAMPLES;
    *cols = 0;

    /* extract command-line arguments */
    while ((c = getopt(argc, argv, options)) != EOF) {
	switch (c) {

	case 'A':		/* sample alignment */
	    Aflag = optarg;
	    break;

	case 'a':		/* interpolate archive */
	    if (++src > 1) {
	    	fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
	    	errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
	    archive = optarg;
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'd':
	    pauseFlag = 1;
	    break;

	case 'f':		/* fixed format count */
	    d = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || d < 0) {
		fprintf(stderr, "%s: -f requires +ve numeric argument\n", pmProgname);
		errflag++;
	   }
	   fixed = d;
	   break;

	case 'g':
	    gui = 1;
	    break;

	case 'h':		/* host name */
	    if (++src > 1) {
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
		errflag++;
	    }
	    cntxt->host = host = optarg;
	    break;

	case 'i':		/* instance names */
	    cntxt->iall = 0;
	    i = cntxt->inum;
	    subopt = getinstance(optarg);
	    while (subopt != NULL) {
		i++;
		cntxt->inames =
		    (char **)realloc(cntxt->inames, i * (sizeof (char *)));
		if (cntxt->inames == NULL) {
		    __pmNoMem("pmval.ip", i * sizeof(char *), PM_FATAL_ERR);
		}
		*(cntxt->inames + i - 1) = subopt;
		subopt = getinstance(NULL);
	    }
	    cntxt->inum = i;
	    break;

	case 'K':	/* update local PMDA table */
	    if ((errmsg = __pmSpecLocalPMDA(optarg)) != NULL) {
		fprintf(stderr, "%s: __pmSpecLocalPMDA failed\n%s\n", pmProgname, errmsg);
		errflag++;
	    }
	    break;

	case 'n':		/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'O':		/* sample offset */
	    Oflag = optarg;
	    break;

	case 'p':		/* port for slave of existing time control */
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || port < 0) {
		fprintf(stderr, "%s: Error: invalid pmtime port \"%s\": %s\n",
			pmProgname, optarg, pmErrStr(-oserror()));
		errflag++;
	    }
	    break;

	case 'r':		/* raw */
	   raw = 1;
	   break;

	case 's':		/* sample count */
	    d = (int)strtol(optarg, &endnum, 10);
	    if (Tflag) {
		fprintf(stderr, "%s: at most one of -s and -T allowed\n", pmProgname);
		errflag++;
	    }
	    else if (*endnum != '\0' || d < 0) {
		fprintf(stderr, "%s: -s requires +ve numeric argument\n", pmProgname);
		errflag++;
	   }
	   else *smpls = d;
	   break;

	case 'S':		/* start run time */
	    Sflag = optarg;
	    break;

	case 't':		/* sampling interval */
	    if (pmParseInterval(optarg, delta, &msg) < 0) {
		fputs(msg, stderr);
		free(msg);
		errflag++;
	    }
	    break;

	case 'T':		/* run time */
	    if (*smpls != ALL_SAMPLES) {
		fprintf(stderr, "%s: at most one of -T and -s allowed\n", pmProgname);
		errflag++;
	    }
	    Tflag = optarg;
	    break;

	case 'U':		/* non-interpolated archive (undocumented) */
	    if (++src > 1) {
	    	fprintf(stderr, "%s: at most one of -a, -h and -U allowed\n", pmProgname);
	    	errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
	    amode = PM_MODE_FORW;
	    archive = optarg;
	    rawarchive = 1;
	    break;

	case 'w':		/* output column width */
	    setoserror(0);
	    d = atol(optarg);
	    if (oserror() || d < 1) errflag++;
	    else *cols = d;
	    break;

	case 'z':	/* timezone from host */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    zflag++;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    tz = optarg;
	    break;

	case '?':
	    fprintf(stderr, usage, pmProgname);
	    exit(EXIT_FAILURE);

	default:
	    errflag++;
	}
    }

    /* parse uniform metric spec */
    if (optind >= argc) {
	fprintf(stderr, "Error: no metricname specified\n\n");
	errflag++;
    }
    else if (optind < argc-1) {
	fprintf(stderr, "Error: pmval can only process one metricname at a time\n\n");
	errflag++;
    }
    else {
	if (ahtype == PM_CONTEXT_HOST) {
	    if (pmParseMetricSpec(argv[optind], 0, host, &msp, &msg) < 0) {
		fputs(msg, stderr);
		free(msg);
		errflag++;
	    }
	}
	else {		/* must be archive */
	    if (pmParseMetricSpec(argv[optind], 1, archive, &msp, &msg) < 0) {
		fputs(msg, stderr);
		free(msg);
		errflag++;
	    }
	}
    }

    if (errflag) {
	fprintf(stderr, usage, pmProgname);
	exit(EXIT_FAILURE);
    }

    if (msp->isarch == 1) {
	archive = msp->source;
	ahtype = PM_CONTEXT_ARCHIVE;
    }
    else if (msp->isarch == 2) {
	ahtype = PM_CONTEXT_LOCAL;
    }

    if (ahtype != PM_CONTEXT_ARCHIVE) {
	if (pauseFlag) {
	    fprintf(stderr, "%s: -d can only be used with -a\n", pmProgname);
	    errflag++;
	}
    }
    else {
	if (gui == 1 && port != -1) {
	    fprintf(stderr, "%s: -g cannot be used with -p\n", pmProgname);
	    errflag++;
	}
	if (gui == 1 && pauseFlag) {
	    fprintf(stderr, "%s: -g cannot be used with -d\n", pmProgname);
	    errflag++;
	}
    }

    if (errflag) {
	fprintf(stderr, usage, pmProgname);
	exit(EXIT_FAILURE);
    }

    cntxt->metric = msp->metric;
    if (msp->ninst > 0) {
	cntxt->inum = msp->ninst;
	cntxt->iall = (cntxt->inum == 0);
	cntxt->inames = &msp->inst[0];
    }

    if (msp->isarch == 1) {
	/* open connection to archive */
	if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, msp->source)) < 0) {
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, msp->source, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	logStart = label.ll_start;
	if ((sts = pmGetArchiveEnd(&last)) < 0) {
	    fprintf(stderr, "%s: Cannot determine end of archive: %s",
		pmProgname, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
    }
    else {
	/* open connection to host or local context */
	if ((sts = pmNewContext(ahtype, msp->source)) < 0) {
	    if (ahtype == PM_CONTEXT_HOST)
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmProgname, msp->source, pmErrStr(sts));
	    else
		fprintf(stderr, "%s: Cannot establish local context: %s\n",
		    pmProgname, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	cntxt->host = msp->source;
	__pmtimevalNow(&logStart);
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmProgname, pmErrStr(tzh));
	    exit(EXIT_FAILURE);
	}
	if (ahtype == PM_CONTEXT_ARCHIVE) {
	    printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
		label.ll_hostname);
	    rpt_tz_label = label.ll_hostname;
	}
	else {
	    printf("Note: timezone set to local timezone of host \"%s\"\n\n", host);
	    rpt_tz_label = host;
	}
    }
    else if (tz != NULL) {
	if ((tzh = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmProgname, tz, pmErrStr(tzh));
	    exit(EXIT_FAILURE);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }
    else printf("\n");

    pmWhichZone(&rpt_tz);
    if (!rpt_tz_label)
	rpt_tz_label = local;

    if (pmParseTimeWindow(Sflag, Tflag, Aflag, Oflag,
			   &logStart, &last,
			   &first, &last, posn, &msg) < 0) {
	fprintf(stderr, "%s", msg);
	free(msg);
	exit(EXIT_FAILURE);
    }

    initapi(cntxt);
    initinsts(cntxt);

    if (!(gui || port != -1) &&
	*smpls == ALL_SAMPLES &&
	last.tv_sec != INT_MAX &&
	amode != PM_MODE_FORW) {

	*smpls = (long)((__pmtimevalToReal(&last) - __pmtimevalToReal(posn)) /
		__pmtimevalToReal(delta));
	if (*smpls < 0) 
	/* if end is before start, no samples thanks */
	    *smpls = 0;
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
	    (*smpls)++;
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    char		tbfr[26];
	    char		*tp;
	    fprintf(stderr, "getargs: first=%.6f", __pmtimevalToReal(&first));
	    tp = pmCtime(&first.tv_sec, tbfr);
	    /*
	     * tp -> Ddd Mmm DD HH:MM:SS YYYY\n
	     *       0   4   8  1      1 2  2 2
	     *                  1      8 0  3 4
	     */
	    fprintf(stderr, "[%8.8s]\n", &tp[11]);
	    fprintf(stderr, "getargs: posn=%.6f", __pmtimevalToReal(posn));
	    tp = pmCtime(&posn->tv_sec, tbfr);
	    fprintf(stderr, "[%8.8s]\n", &tp[11]);
	    fprintf(stderr, "getargs: last=%.6f", __pmtimevalToReal(&last));
	    tp = pmCtime(&last.tv_sec, tbfr);
	    fprintf(stderr, "[%8.8s]\n", &tp[11]);
	    fprintf(stderr, "getargs: delta=%.6f samples=%ld\n",
	    __pmtimevalToReal(delta), *smpls);
	}
#endif
    }

    if (gui || port != -1) {
	/* set up pmtime control */
	pmtime = pmTimeStateSetup(&controls, ahtype, port, *delta, *posn,
				    first, last, rpt_tz, rpt_tz_label);
	controls.stepped = timestep;
	gui = 1;	/* we're using pmtime control from here on */
    }
    else if (ahtype == PM_CONTEXT_ARCHIVE) /* no time control, go it alone */
	pmTimeStateMode(amode, *delta, posn);
}

/***************************************************************************
 * main
 ***************************************************************************/
int
main(int argc, char *argv[])
{
    struct timeval  delta = { 0 };	/* sample interval */
    long	    smpls;		/* number of samples */
    int             cols;		/* width of output column */
    struct timeval  now = { 0 };	/* current task start time */
    Context	    cntxt = { 0 };	/* performance metric description */
    pmResult	    *rslt1;		/* current values */
    pmResult	    *rslt2;		/* previous values */
    int		    forever;
    int		    idx1;
    int		    idx2 = 0;		/* initialize to pander to gcc */
    int		    no_values = 0;

    __pmSetProgname(argv[0]);
    setlinebuf(stdout);

    getargs(argc, argv, &cntxt, &now, &delta, &smpls, &cols);

    if (cntxt.desc.type == PM_TYPE_EVENT) {
	fprintf(stderr, "%s: Cannot display values for PM_TYPE_EVENT metrics\n",
		pmProgname);
	exit(EXIT_FAILURE);
    }

    forever = (smpls == ALL_SAMPLES || gui);

    if (cols <= 0) cols = howide(cntxt.desc.type);

    if ((fixed == 0 && fixed > cols) || (fixed > 0 && fixed > cols - 2)) {
	fprintf(stderr, "%s: -f %d too large for column width %d\n",
		pmProgname, fixed, cols);
	exit(EXIT_FAILURE);
    }

    printhdr(&cntxt, smpls, delta, now);

    /* wait till time for first sample */
    if (archive == NULL)
	__pmtimevalPause(now);

    /* main loop fetching and printing sample values */
    while (forever || (smpls-- > 0)) {
	if (gui)
	    pmTimeStateVector(&controls, pmtime);
	if (havePrev == 0) {
	    /*
	     * We don't yet have a value at the previous time point ...
	     * save this value so we can use it to compute the rate if
	     * the metric has counter semantics and we're doing rate
	     * conversion.
	     */
	    if ((idx2 = getvals(&cntxt, &rslt2)) >= 0) {
		/* previous value success */
		havePrev = 1;
		if (cntxt.desc.indom != PM_INDOM_NULL)
		    printlabels(&cntxt, cols);
		if (raw || (cntxt.desc.sem != PM_SEM_COUNTER)) {
		    /* not doing rate conversion, report this value immediately */
		    if (gui || archive != NULL)
			__pmPrintStamp(stdout, &rslt2->timestamp);
		    printvals(&cntxt, rslt2->vset[idx2], cols);
		    continue;
		}
		else if (no_values) {
		    if (gui || archive != NULL) {
			__pmPrintStamp(stdout, &rslt2->timestamp);
			printf("  ");
		    }
		    printf("No values available\n");
		}
		no_values = 0;
		if (gui)
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
	if (!gui && (pauseFlag || archive == NULL))
	    __pmtimevalSleep(delta);

	if (havePrev == 0)
	    continue;	/* keep trying to get the previous sample */

	/* next sample */
	if ((idx1 = getvals(&cntxt, &rslt1)) == -2)
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
	if (cntxt.iall && ! chkinsts(&cntxt, rslt1->vset[idx1])) {
	    free(cntxt.iids);
	    if (cntxt.iall)
		free(cntxt.inames);
	    free(cntxt.ipairs);
	    initinsts(&cntxt);
	    printlabels(&cntxt, cols);
	}

	/* print values */
	if (gui || archive != NULL)
	    __pmPrintStamp(stdout, &rslt1->timestamp);
	if (raw || (cntxt.desc.sem != PM_SEM_COUNTER))
	    printvals(&cntxt, rslt1->vset[idx1], cols);
	else
	    printrates(&cntxt, rslt1->vset[idx1], rslt1->timestamp,
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
