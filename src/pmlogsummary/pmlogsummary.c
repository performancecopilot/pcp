/*
 * Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <math.h>
#include <stdarg.h>
#include <limits.h>
#include "pmapi.h"
#include "impl.h"

static void
usage(void)
{
    fprintf(stderr,
"Usage: %s [options] archive [metricname ...]\n\n"
"Options:\n"
"  -a            print all information (equivalent to -blmMy)\n"
"  -b            print both stochastic and time averages for counter metrics\n"
"  -B nbins      print value distribution across a number of bins\n"
"  -f            print using \"spreadsheet\" format (tab delimited fields)\n"
"  -F            print using \"spreadsheet\" format (comma separated values)\n"
"  -i            also print timestamp for minimum value\n"
"  -I            also print timestamp for maximum value\n"
"  -l            also print the archive label\n"
"  -m            also print minimum values\n"
"  -M            also print maximum values\n"
"  -n pmnsfile   use an alternative PMNS\n"
"  -N            suppress warnings from individual archive fetches [default]\n"
"  -p precision  number of digits to display after the decimal point\n"
"  -S starttime  start of the time window\n"
"  -T endtime    end of the time window\n"
"  -v            verbose, enable warnings from individual archive fetches\n"
"  -x            print only stochastic averages for counter metrics\n"
"  -y            print sample count for each metric\n"
"  -z            set reporting timezone to local time of metrics source\n"
"  -Z timezone   set reporting timezone\n",
	pmProgname);
    exit(1);
}


typedef struct {
    int			inst;
    unsigned int	count;
    double		stocave;	/* stochastic average */
    double		timeave;	/* time average */
    double		min;		/* minimum value */
    double		max;		/* maximum value */
    double		sum;		/* sum of all values */
    double		lastval;	/* value from previous sample */
    struct timeval	firsttime;	/* time of first sample */
    struct timeval	lasttime;	/* time of previous sample */
    struct timeval	mintime;	/* time of minimum sample */
    struct timeval	maxtime;	/* time of maximum sample */
    int			markcount;	/* num mark records seen */
    int			marked;		/* seen since last "mark" record? */
    unsigned int	bintotal;	/* copy of count for 2nd pass */
    unsigned int	*bin;		/* bins for value distribution */
} instData;

typedef struct {
    pmDesc		desc;
    double		scale;
    instData		**instlist;
    unsigned int	listsize;
} aveData;

/*
 * Hash control for statistics & errors related to each metric
 */
static __pmHashCtl	hashlist;
static __pmHashCtl	errlist;

/* output format flags */
static unsigned int	stocaveflag;	/* no stochastic counter ave */
static unsigned int	timeaveflag = 1;/* use time counter ave */
static unsigned int	maxflag;	/* no maximum */
static unsigned int	minflag;	/* no minimum */
static unsigned int	sumflag;	/* no sum */
static unsigned int	maxtimeflag;	/* no timestamp for maximum */
static unsigned int	mintimeflag;	/* no timestamp for minimum */
static unsigned int	countflag;	/* no count */
static unsigned int	warnflag;	/* warnings are off by default */
static unsigned int	delimiter = ' ';/* output field separator */
static unsigned int	nbins;		/* number of distribution bins */
static unsigned int	precision = 3;	/* number of digits after "." */

/* time window stuff */
static int		sflag;
static int		tflag;
static int		dayflag;
static struct timeval	logstart;
static struct timeval   logend;
static struct timeval	windowstart;
static struct timeval   windowend;
static char		timebuf[32];		/* for pmCtime result + .xxx */

/* duration of log */
static double		logspan;

/* optional metric specification, optionally with instances */
pmMetricSpec		*msp;

/* time manipulation */
static int
tsub(struct timeval *a, struct timeval *b)
{
    if ((a == NULL) || (b == NULL))
	return -1;
    a->tv_usec -= b->tv_usec;
    if (a->tv_usec < 0) {
	a->tv_usec += 1000000;
	a->tv_sec--;
    }
    a->tv_sec -= b->tv_sec;
    if (a->tv_sec < 0) {
	/* clip negative values at zero */
	a->tv_sec = 0;
	a->tv_usec = 0;
    }
    return 0;
}

static int
tadd(struct timeval *a, struct timeval *b)
{
    if ((a == NULL) || (b == NULL))
	return -1;
    a->tv_usec += b->tv_usec;
    if (a->tv_usec > 1000000) {
	a->tv_usec -= 1000000;
	a->tv_sec++;
    }
    a->tv_sec += b->tv_sec;
    return 0;
}

static double
tosec(struct timeval t)
{
    return t.tv_sec + (t.tv_usec / 1000000.0);
}

static void
pmiderr(pmID pmid, const char *msg, ...)
{
    if (warnflag && __pmHashSearch(pmid, &errlist) == NULL) {
	va_list	arg;
	char	*mname = NULL;
	static char *unknown = "(cannot find metric name) ";

	pmNameID(pmid, &mname);
	if (!mname) mname = unknown;
	fprintf(stderr, "%s: %s(%s) - ", pmProgname, mname, pmIDStr(pmid));
	va_start(arg, msg);
	vfprintf(stderr, msg, arg);
	va_end(arg);
	__pmHashAdd(pmid, NULL, &errlist);
	if (mname && mname != unknown) free(mname);
    }
}

static void
printstamp(struct timeval *stamp, int delimiter)
{
    if (dayflag) {
	char	*ddmm;
	char	*yr;

	ddmm = pmCtime(&stamp->tv_sec, timebuf);
	ddmm[10] = ' ';
	ddmm[11] = '\0';
	yr = &ddmm[20];
	printf("%c'%s", delimiter, ddmm);
	__pmPrintStamp(stdout, stamp);
	printf(" %4.4s\'", yr);
    }
    else {
	printf("%c", delimiter);
	__pmPrintStamp(stdout, stamp);
    }
}

static void
printheaders(void)
{
    printf("metric");
    if (stocaveflag)
	printf("%cstochastic_average", delimiter);
    if (timeaveflag)
	printf("%ctime_average", delimiter);
    if (sumflag)
	printf("%csum", delimiter);
    if (minflag)
	printf("%cminimum", delimiter);
    if (mintimeflag)
	printf("%cminimum_time", delimiter);
    if (maxflag)
	printf("%cmaximum", delimiter);
    if (maxtimeflag)
	printf("%cmaximum_time", delimiter);
    if (countflag)
	printf("%ccount", delimiter);
    if (nbins)
	printf("%cbins", delimiter);
    printf("%cunits\n", delimiter);
}

static void
printsummary(const char *name)
{
    int			sts;
    int			i, j;
    int			star;
    pmID		pmid;
    char		*str = NULL;
    const char		*u;
    __pmHashNode	*hptr;
    aveData		*avedata;
    instData		*instdata;
    double		metricspan = 0.0;
    struct timeval	metrictimespan;

    /* cast away const, pmLookupName should never modify name */
    if ((sts = pmLookupName(1, (char **)&name, &pmid)) < 0) {
	fprintf(stderr, "%s: failed to lookup metric name (pmid=%s): %s\n",
		pmProgname, name, pmErrStr(sts));
	return;
    }

    /* lookup using pmid, print values according to set flags */
    if ((hptr = __pmHashSearch(pmid, &hashlist)) != NULL) {
	avedata = (aveData*)hptr->data;
	for (i = 0; i < avedata->listsize; i++) {
	    if ((instdata = avedata->instlist[i]) == NULL)
		break;
	    if (avedata->desc.sem == PM_SEM_DISCRETE) {
		double		val;
		struct timeval	timediff;

		/* extend discrete metrics to the archive end */
		timediff = logend;
		tsub(&timediff, &instdata->lasttime);
		val = instdata->lastval;
		instdata->stocave += val;
		instdata->timeave += val*tosec(timediff);
		instdata->lasttime = logend;
		instdata->count++;
	    }
	    metrictimespan = instdata->lasttime;
	    tsub(&metrictimespan, &instdata->firsttime);
	    metricspan = tosec(metrictimespan);
	    /* counter metric doesn't cover 90% of log */
	    star = (avedata->desc.sem == PM_SEM_COUNTER && metricspan / logspan <= 0.1);

	    if ((sts = pmNameInDom(avedata->desc.indom, instdata->inst, &str)) < 0) {
		if (msp && msp->ninst > 0 && avedata->desc.indom == PM_INDOM_NULL)
		    break;
		if (star)
		    putchar('*');
		if (avedata->desc.indom == PM_INDOM_NULL)
		    printf("%s%c", name, delimiter);
		else
		    printf("%s%c[%u]", name, delimiter, instdata->inst);
	    }
	    else {	/* part of an instance domain */
		if (msp && msp->ninst > 0) {
		    for (j = 0; j < msp->ninst; j++)
			if (strcmp(msp->inst[j], str) == 0)
			    break;
		    if (j == msp->ninst)
			continue;
		}
		if (star)
		    putchar('*');
		printf("%s%c[\"%s\"]", name, delimiter, str);
	    }
	    if (str) free(str);
	    str = NULL;

	    /* complete the calculations, count is number of intervals */
	    if (avedata->desc.sem == PM_SEM_COUNTER) {
		if (metricspan <= 0) {
		    printf("%c- insufficient archive data.\n", delimiter);
		    continue;
		}
		instdata->stocave /= (double)instdata->count;
		instdata->timeave /= metricspan * avedata->scale;
	    }
	    else {	/* non-counters, count is number of observations */
		instdata->stocave /= (double)instdata->count;
		if (metricspan == 0)	/* happens for instantaneous metrics */
		    metricspan = 1;	/* only - just report the one value  */
		instdata->timeave /= metricspan;
	    }

	    if (stocaveflag)
		printf("%c%.*f", delimiter, (int)precision, instdata->stocave);
	    if (timeaveflag)
		printf("%c%.*f", delimiter, (int)precision, instdata->timeave);
	    if (sumflag)
		printf("%c%.*f", delimiter, (int)precision, instdata->sum);
	    if (minflag)
		printf("%c%.*f", delimiter, (int)precision, instdata->min);
	    if (mintimeflag)
		printstamp(&instdata->mintime, delimiter);
	    if (maxflag)
		printf("%c%.*f", delimiter, (int)precision, instdata->max);
	    if (maxtimeflag)
		printstamp(&instdata->maxtime, delimiter);
	    if (avedata->desc.sem == PM_SEM_DISCRETE)	/* all added marks + added endpoint above */
		instdata->count = instdata->count - instdata->markcount - 1;
	    if (countflag)
		printf("%c%u", delimiter, instdata->count);
	    for (j=0; j < nbins; j++) {	/* print value distribution summary */
		if (j > 0 && instdata->min == instdata->max)	/* all in 1st bin */
		    printf("%c[]%c%u", delimiter, delimiter, 0);
		else
		    printf("%c[<=%.*f]%c%u", delimiter, (int)precision,
			((instdata->max - instdata->min) / nbins * (j+1)) + instdata->min,
			delimiter, instdata->bin[j]);
	    }
	    u = pmUnitsStr(&avedata->desc.units);
	    printf("%c%s\n", delimiter, *u == '\0' ? "none" : u);
	    if (instdata) {
		if (instdata->bin)
		    free(instdata->bin);
		free(instdata);
	    }
	}
	if (avedata->instlist) free(avedata->instlist);
	__pmHashDel(avedata->desc.pmid, (void*)avedata, &hashlist);
	free(avedata);
    }
}

static double
unwrap(double current, double previous, int pmtype)
{
    double	outval = current;
    static int	dowrap = -1;

    if ((current - previous) < 0.0) {
	if (dowrap == -1) {
	    /* PCP_COUNTER_WRAP in environment enables "counter wrap" logic */
	    if (getenv("PCP_COUNTER_WRAP") == NULL)
		dowrap = 0;
	    else
		dowrap = 1;
	}
	if (dowrap) {
	    switch (pmtype) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    outval += (double)UINT_MAX+1;
		    break;
		case PM_TYPE_64:
		case PM_TYPE_U64:
		    outval += (double)ULONGLONG_MAX+1;
		    break;
	    }
	}
    }

    return outval;
}

static void
newHashInst(pmValue *vp,
	aveData *avedata,		/* updated by this function */
	int valfmt,
	struct timeval *timestamp,	/* timestamp for this sample */
	int pos)			/* position of this inst in instlist */
{
    int		sts;
    size_t	size;
    instData	*instdata;
    pmAtomValue av;

    if ((sts = pmExtractValue(valfmt, vp, avedata->desc.type, &av, PM_TYPE_DOUBLE)) < 0) {
	pmiderr(avedata->desc.pmid, "failed to extract value: %s\n", pmErrStr(sts));
	fprintf(stderr, "%s: possibly corrupt archive?\n", pmProgname);
	exit(1);
    }
    size = (pos+1) * sizeof(instData *);
    avedata->instlist = (instData **) realloc(avedata->instlist, size);
    if (avedata->instlist == NULL)
	__pmNoMem("newHashInst.instlist", size, PM_FATAL_ERR);
    size = sizeof(instData);
    avedata->instlist[pos] = instdata = (instData *) malloc(size);
    if (instdata == NULL)
	__pmNoMem("newHashInst.instlist[inst]", size, PM_FATAL_ERR);
    if (nbins == 0)
	instdata->bin = NULL;
    else {	/* we are doing binning ... make space for the bins */
	size = nbins * sizeof(unsigned int);
	instdata->bin = (unsigned int *)malloc(size);
	if (instdata->bin == NULL)
	    __pmNoMem("newHashInst.instlist[inst].bin", size, PM_FATAL_ERR);
	memset(instdata->bin, 0, size);
    }
    instdata->inst = vp->inst;
    if (avedata->desc.sem == PM_SEM_COUNTER) {
	instdata->min = 0.0;
	instdata->max = 0.0;
	instdata->sum = 0.0;
	instdata->mintime = *timestamp;
	instdata->maxtime = *timestamp;
	instdata->stocave = 0.0;
	instdata->timeave = 0.0;
	instdata->count = 0;
    }
    else {	/* for the other semantics */
	instdata->min = av.d;
	instdata->max = av.d;
	instdata->sum = av.d;
	instdata->mintime = *timestamp;
	instdata->maxtime = *timestamp;
	instdata->stocave = av.d;
	instdata->timeave = 0.0;
	instdata->count = 1;
    }
    instdata->marked = 0;
    instdata->bintotal = 0;
    instdata->markcount = 0;
    instdata->lastval = av.d;
    instdata->firsttime = *timestamp;
    instdata->lasttime = *timestamp;
    avedata->listsize++;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	char	*name = NULL;
	pmNameID(avedata->desc.pmid, &name);
	fprintf(stderr, "%s Initially - min/max=%f/%f\n", name,
		instdata->min, instdata->max);
	if (name) free(name);
    }
#endif
}

static void
newHashItem(pmValueSet *vsp,
	pmDesc *desc,
	aveData *avedata,		/* output from this function */
	struct timeval *timestamp)	/* timestamp for this sample */
{
    int j;

    avedata->desc = *desc;
    avedata->scale = 0.0;

    /* convert counter metric units to rate units & get time scale */
    if (avedata->desc.sem == PM_SEM_COUNTER && !sumflag) {
	if (avedata->desc.units.dimTime == 0)
	    avedata->scale = 1.0;
	else {
	    if (avedata->desc.units.scaleTime > PM_TIME_SEC)
		avedata->scale = pow(60.0, (double)(PM_TIME_SEC - avedata->desc.units.scaleTime));
	    else
		avedata->scale = pow(1000.0, (double)(PM_TIME_SEC - avedata->desc.units.scaleTime));
	}
	if (avedata->desc.units.dimTime == 0)
	    avedata->desc.units.scaleTime = PM_TIME_SEC;
	avedata->desc.units.dimTime--;
    }
    else if (avedata->desc.sem == PM_SEM_COUNTER && sumflag) {
	avedata->scale = 1.0;
    }
    avedata->listsize = 0;
    avedata->instlist = NULL;
    for (j = 0; j < vsp->numval; j++)
	newHashInst(&vsp->vlist[j], avedata, vsp->valfmt, timestamp, j);
}

/*
 * find index to bin array for "val"
 */
unsigned int
findbin(pmID pmid, double val, double min, double max)
{
    unsigned int	index;
    double		bound, next;
    double		binsize;

    binsize = (max - min) / (double)nbins;
    bound = min;
    for (index=0; index < nbins-1; index++) {
	next = bound + binsize;
	if (val >= bound && val <= next)
	    break;
	bound = next;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	char *mname = NULL;
	pmNameID(pmid, &mname);
	fprintf(stderr, "%s selected bin %u/%u (val=%.*f, min=%.*f, max=%.*f)\n",
		mname, index, nbins, (int)precision, val, (int)precision,
		min, (int)precision, max);
	if (mname) free(mname);
	if (index >= nbins) exit(1);
    }
#endif
    return index;
}

/*
 * must keep a note for every instance of every metric whenever a mark
 * record has been seen between now & the last fetch for that instance
 */
static void
markrecord(pmResult *result)
{
    int			i, j;
    __pmHashNode	*hptr;
    aveData		*avedata;
    instData		*instdata;
    double		val;
    struct timeval	timediff;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	printstamp(&result->timestamp, '\n');
	printf(" - mark record\n\n");
    }
#endif
    for (i = 0; i < hashlist.hsize; i++) {
	for (hptr = hashlist.hash[i]; hptr != NULL; hptr = hptr->next) {
	    avedata = (aveData *)hptr->data;
	    for (j = 0; j < avedata->listsize; j++) {
		instdata = avedata->instlist[j];
		if (avedata->desc.sem == PM_SEM_DISCRETE) {
		    /* extend discrete metrics to the mark point */
		    timediff = result->timestamp;
		    tsub(&timediff, &instdata->lasttime);
		    val = instdata->lastval;
		    instdata->stocave += val;
		    instdata->timeave += val*tosec(timediff);
		    instdata->lasttime = result->timestamp;
		    instdata->count++;
		}
		instdata->marked = 1;
		instdata->markcount++;
	    }
	}
    }
}

static void
calcbinning(pmResult *result)
{
    int			i, j, k;
    int			sts;
    int			wrap;
    double		val;
    pmAtomValue 	av;
    pmValue		*vp;
    pmValueSet		*vsp;
    __pmHashNode	*hptr = NULL;
    aveData		*avedata = NULL;
    instData		*instdata;
    double		diff;
    struct timeval	timediff;

    if (result->numpmid == 0)	/* mark record */
	markrecord(result);

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	if (vsp->numval == 0)
	    continue;
	else if (vsp->numval < 0) {
	    pmiderr(vsp->pmid, "failed in 2nd pass archive fetch: %s\n", pmErrStr(vsp->numval));
	    continue;
	}

	if ((hptr = __pmHashSearch(vsp->pmid, &hashlist)) != NULL) {
	    avedata = (aveData *)hptr->data;
	    for (j = 0; j < vsp->numval; j++) {	/* iterate thro result values */
		int	fp_bad;
		vp = &vsp->vlist[j];
		k = j;	/* index into stored inst list, result may differ */
		if ((vsp->numval > 1) || (avedata->desc.indom != PM_INDOM_NULL)) {
		    if ((k < avedata->listsize) && (avedata->instlist[k]->inst != vp->inst)) {
			for (k = 0; k < avedata->listsize; k++) {
			    if (vp->inst == avedata->instlist[k]->inst)
				break;	/* k now correct */
			}
			if (k == avedata->listsize) {
			    pmiderr(vsp->pmid, "ignoring new instance found on second pass\n");
			    continue;
			}
		    }
		    else if (k >= avedata->listsize) {
			k = avedata->listsize;
			pmiderr(vsp->pmid, "ignoring new instance found on second pass\n");
			continue;
		    }
		}
		instdata = avedata->instlist[k];

		if ((sts = pmExtractValue(vsp->valfmt, vp, avedata->desc.type, &av, PM_TYPE_DOUBLE)) < 0) {
		    pmiderr(avedata->desc.pmid, "failed to extract value: %s\n", pmErrStr(sts));
		    continue;
		}
		fp_bad = 0;
#ifdef HAVE_FPCLASSIFY
		fp_bad = fpclassify(av.d) == FP_NAN;
#else
#ifdef HAVE_ISNAN
		fp_bad = isnan(av.d);
#endif
#endif
		if (fp_bad)
		    continue;

		/* reset values from first pass needed in this second pass */
		if (instdata->bintotal == 0) {	/* 1st time instance seen on 2nd pass */
		    instdata->bintotal = instdata->count;
		    instdata->lasttime = result->timestamp;
		    instdata->firsttime = result->timestamp;
		    instdata->lastval = av.d;
		    if (avedata->desc.sem == PM_SEM_COUNTER)
			instdata->count = 0;
		    else {
			unsigned int sts;
			instdata->count = 1;
			sts = findbin(avedata->desc.pmid, av.d, instdata->min, instdata->max);
			instdata->bin[sts]++;
		    }
		    continue;
		}

		timediff = result->timestamp;
		tsub(&timediff, &instdata->lasttime);
		diff = tosec(timediff);
		wrap = 0;
		if (avedata->desc.sem == PM_SEM_COUNTER) {
		    diff *= avedata->scale;
		    if (diff == 0.0) continue;
		    if (instdata->marked)
			val = av.d;
		    else
			val = unwrap(av.d, instdata->lastval, avedata->desc.type);
		    if (instdata->marked || val < instdata->lastval) {
			/* mark or not first one & counter not monotonic increasing */
			wrap = 1;
			instdata->marked = 0;
			tadd(&instdata->firsttime, &result->timestamp);
			tsub(&instdata->firsttime, &instdata->lasttime);
		    }
		    else {
			unsigned int	sts;
			val = (val - instdata->lastval) / diff;
			sts = findbin(avedata->desc.pmid, val, instdata->min, instdata->max);
			instdata->bin[sts]++;
		    }
		}
		else {	/* for the other semantics */
		    unsigned int	sts;
		    val = av.d;
		    sts = findbin(avedata->desc.pmid, val, instdata->min, instdata->max);
		    instdata->bin[sts]++;
		}
		if (!wrap) {
		    instdata->count++;
		}
		instdata->lastval = av.d;
		instdata->lasttime = result->timestamp;
	    }
	}
    }
}

static void
calcaverage(pmResult *result)
{
    int			i, j, k;
    int			sts;
    int			wrap;
    double		val;
    pmDesc		desc;
    pmAtomValue 	av;
    pmValue		*vp;
    pmValueSet		*vsp;
    __pmHashNode	*hptr = NULL;
    aveData		*avedata = NULL;
    instData		*instdata;
    double		diff;
    double		rate = 0;
    struct timeval	timediff;

    if (result->numpmid == 0)	/* mark record */
	markrecord(result);

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	if (vsp->numval == 0)
	    continue;
	else if (vsp->numval < 0) {
	    pmiderr(vsp->pmid, "failed in archive value fetch: %s\n", pmErrStr(vsp->numval));
	    continue;
	}

	/* check if pmid already in hash list */
	if ((hptr = __pmHashSearch(vsp->pmid, &hashlist)) == NULL) {
	    if ((sts = pmLookupDesc(vsp->pmid, &desc)) < 0) {
		pmiderr(vsp->pmid, "cannot find descriptor: %s\n", pmErrStr(sts));
		continue;
	    }

	    if (desc.type != PM_TYPE_32 && desc.type != PM_TYPE_U32 &&
		desc.type != PM_TYPE_64 && desc.type != PM_TYPE_U64 &&
		desc.type != PM_TYPE_FLOAT && desc.type != PM_TYPE_DOUBLE) {
		continue;	/* cannot average non-numeric metrics */
	    }

	    /* create a new one & add to list */
	    avedata = (aveData*) malloc(sizeof(aveData));
	    newHashItem(vsp, &desc, avedata, &result->timestamp);
	    if (__pmHashAdd(avedata->desc.pmid, (void*)avedata, &hashlist) < 0) {
		pmiderr(avedata->desc.pmid, "failed %s hash table insertion\n", pmProgname);
		/* free memory allocated above on insert failure */
		for (j = 0; j < vsp->numval; j++)
		    if (avedata->instlist[j]) free(avedata->instlist[j]);
		if (avedata->instlist) free(avedata->instlist);
		continue;
	    }
	}
	else {	/* pmid exists - update statistics */
	    avedata = (aveData*)hptr->data;
	    for (j = 0; j < vsp->numval; j++) {	/* iterate thro result values */
		int	fp_bad;
		vp = &vsp->vlist[j];
		k = j;	/* index into stored inst list, result may differ */
		if ((vsp->numval > 1) || (avedata->desc.indom != PM_INDOM_NULL)) {
		    /* must store values using correct inst - probably in correct order already */
		    if ((k < avedata->listsize) && (avedata->instlist[k]->inst != vp->inst)) {
			for (k = 0; k < avedata->listsize; k++) {
			    if (vp->inst == avedata->instlist[k]->inst) {
				break;	/* k now correct */
			    }
			}
			if (k == avedata->listsize) {	/* no matching inst was found */
			    newHashInst(vp, avedata, vsp->valfmt, &result->timestamp, k);
			    continue;
			}
		    }
		    else if (k >= avedata->listsize) {
			k = avedata->listsize;
			newHashInst(vp, avedata, vsp->valfmt, &result->timestamp, k);
			continue;
		    }
		}
		instdata = avedata->instlist[k];

		if ((sts = pmExtractValue(vsp->valfmt, vp, avedata->desc.type, &av, PM_TYPE_DOUBLE)) < 0) {
		    pmiderr(avedata->desc.pmid, "failed to extract value: %s\n", pmErrStr(sts));
		    continue;
		}
		fp_bad = 0;
#ifdef HAVE_FPCLASSIFY
		fp_bad = fpclassify(av.d) == FP_NAN;
#else
#ifdef HAVE_ISNAN
		fp_bad = isnan(av.d);
#endif
#endif
		if (fp_bad)
		    continue;
		timediff = result->timestamp;
		tsub(&timediff, &instdata->lasttime);
		diff = tosec(timediff);
		wrap = 0;
		if (avedata->desc.sem == PM_SEM_COUNTER) {
		    diff *= avedata->scale;
		    if (diff == 0.0) continue;
		    if (instdata->marked)
			val = av.d;
		    else
			val = unwrap(av.d, instdata->lastval, avedata->desc.type);
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0) {
			char	*name = NULL;
			pmNameID(avedata->desc.pmid, &name);
			fprintf(stderr, "%s base value is %f, count %d\n",
				name, val, instdata->count+1);
			if (name) free(name);
		    }
#endif
		    if (instdata->marked || val < instdata->lastval) {
			/* either previous record was a "mark", or this is not */
			/* the first one, and counter not monotonic increasing */
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_APPL1) {
			    char	*name = NULL;
			    pmNameID(avedata->desc.pmid, &name);
			    fprintf(stderr, "%s counter wrapped or <mark>\n", name);
			    if (name) free(name);
			}
#endif
			wrap = 1;
			instdata->marked = 0;
			tadd(&instdata->firsttime, &result->timestamp);
			tsub(&instdata->firsttime, &instdata->lasttime);
		    }
		    else {
			rate = (val - instdata->lastval) / diff;
			instdata->stocave += rate;
			if (!instdata->marked)
			    instdata->timeave += (val - instdata->lastval);
			else {
			    instdata->marked = 0;
			    /* remove the timeslice in question from time-based calc */
			    tadd(&instdata->firsttime, &result->timestamp);
			    tsub(&instdata->firsttime, &instdata->lasttime);
			}
			if (instdata->count == 0) {		/* 1st time */
			    instdata->min = instdata->max = rate;
			    instdata->sum = (val - instdata->lastval);
			}
			else {
#ifdef PCP_DEBUG
			    if (pmDebug & DBG_TRACE_APPL2) {
				char	*name = NULL;
				char	*istr = NULL;

				pmNameID(avedata->desc.pmid, &name);
				if (pmNameInDom(avedata->desc.indom,
				    instdata->inst, &istr) < 0)
				    istr = NULL;
				if (rate < instdata->min) {
				    fprintf(stderr, "new min value for %s "
						    "(inst[%s]: %f) at ",
					name, (istr == NULL ? "":istr), rate);
				    __pmPrintStamp(stderr, &result->timestamp);
				    fprintf(stderr, "\n");
				}
				if (rate > instdata->max) {
				    fprintf(stderr, "new max value for %s "
						    "(inst[%s]: %f) at ",
					name, (istr == NULL ? "":istr), rate);
				    __pmPrintStamp(stderr, &result->timestamp);
				    fprintf(stderr, "\n");
				}
				if (name) free(name);
				if (istr) free(istr);
			    }
#endif
			    if (rate < instdata->min) {
				instdata->min = rate;
				instdata->mintime = result->timestamp;
			    }
			    if (rate > instdata->max) {
				instdata->max = rate;
				instdata->maxtime = result->timestamp;
			    }
			    instdata->sum += (val - instdata->lastval);
			}
		    }
		}
		else {	/* for the other semantics - discrete & instantaneous */
		    val = av.d;
		    instdata->sum += val;
		    instdata->stocave += val;
		    if (val < instdata->min) {
			instdata->min = val;
			instdata->mintime = result->timestamp;
		    }
		    if (val > instdata->max) {
			instdata->max = val;
			instdata->maxtime = result->timestamp;
		    }
		    if (!instdata->marked)
			instdata->timeave += instdata->lastval*diff;
		    else {
			instdata->marked = 0;
			/* remove the timeslice in question from time-based calc */
			tadd(&instdata->firsttime, &result->timestamp);
			tsub(&instdata->firsttime, &instdata->lasttime);
		    }
		}
		if (!wrap) {
		    instdata->count++;
#ifdef PCP_DEBUG
		    if ((pmDebug & DBG_TRACE_APPL1) &&
			(avedata->desc.sem != PM_SEM_COUNTER || instdata->count > 0)) {
			char	*name = NULL;
			double	metricspan = 0.0;
			struct timeval	metrictimespan;

			metrictimespan = result->timestamp;
			tsub(&metrictimespan, &instdata->firsttime);
			metricspan = tosec(metrictimespan);
			pmNameID(avedata->desc.pmid, &name);

			if (avedata->desc.sem == PM_SEM_COUNTER) {
			    fprintf(stderr, "++ %s timedelta=%f count=%d\n"
					    "sum=%f min=%f max=%f stocsum=%f\n"
					    "rate=%f timesum=%f (+%f) timespan=%f\n",
				    name, diff, instdata->count, instdata->sum,
				    instdata->min, instdata->max,
				    instdata->stocave, rate, instdata->timeave,
				    diff * (val - instdata->lastval) / 2,
				    metricspan);
			}
			else {	/* non-counters */
			    fprintf(stderr, "++ %s timedelta=%f count=%d\n"
					    "sum=%f min=%f max=%f stocsum=%f\n"
					    "lastval=%f timesum=%f (+%f) timespan=%f\n",
				    name, diff, instdata->count, instdata->sum,
				    instdata->min, instdata->max,
				    instdata->stocave, instdata->lastval,
				    instdata->timeave, instdata->lastval*diff,
				    metricspan);
			}
			if (name)
			    free(name);
		    }
#endif
		}
		instdata->lastval = av.d;
		instdata->lasttime = result->timestamp;
	    }
	}
    }
}


int
main(int argc, char *argv[])
{
    int			c, i, sts, trip, exitstatus=0;
    char		*pmnsfile = PM_NS_DEFAULT;
    char		*startstr = NULL;
    char		*endstr = NULL;
    char		*endnum = NULL;
    char		*msg = NULL;
    int			errflag = 0;
    int			lflag = 0;		/* no label by default */
    int			Hflag = 0;		/* no header by default */
    pmResult		*result;
    pmLogLabel		label;			/* get hostname for archives */
    struct timeval 	timespan = {0, 0};
    int			zflag = 0;		/* for -z */
    char 		*tz = NULL;		/* for -Z timezone */
    char		*archive;
    char		timebuf[32];		/* for pmCtime result + .xxx */

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "abB:D:fFHiIlmMNn:p:rsS:T:vxyzZ:?")) != EOF) {
	switch (c) {

	case 'a':	/* provide all information */
	    stocaveflag = timeaveflag = lflag = countflag = minflag = maxflag = 1;
	    sumflag = 0;
	    break;

	case 'b':	/* use both averages */
	    stocaveflag = 1;
	    timeaveflag = 1;
	    sumflag = 0;
	    break;

	case 'B':	/* number of distribution bins */
	    sts = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || sts < 0) {
		fprintf(stderr, "%s: -B requires positive numeric argument\n", pmProgname);
		errflag++;
	    }
	    else
		nbins = (unsigned int)sts;
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

	case 'f':	/* spreadsheet format - use tab delimiters */
	    delimiter = '\t';
	    break;

	case 'F':	/* spreadsheet format - use comma delimiters */
	    delimiter = ',';
	    break;

	case 'H':	/* print columns headings */
	    Hflag = 1;
	    break;

	case 'i':	/* print timestamp for minimum */
	    mintimeflag = 1;
	    break;

	case 'I':	/* print timestamp for maximum */
	    maxtimeflag = 1;
	    break;

	case 'l':	/* display label */
	    lflag = 1;
	    break;

	case 'm':	/* print minimums */
	    minflag = 1;
	    break;

	case 'M':	/* print maximums */
	    maxflag = 1;
	    break;

	case 'N':	/* suppress fetch warnings */
	    warnflag = 0;
	    break;

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'p':	/* number of digits after decimal point */
	    precision = (unsigned int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -p requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 's':	/* print sums (and only sums) */
	    stocaveflag = timeaveflag = lflag = countflag = minflag = maxflag = 0;
	    sumflag = 1;
	    break;

	case 'S':
	    startstr = optarg;
	    sflag = 1;
	    break;

	case 'T':
	    endstr = optarg;
	    tflag = 1;
	    break;

	case 'v':	/* verbose "fetch" warnings reported */
	    warnflag = 1;
	    break;

	case 'x':	/* use only stochastic counter averages */
	    stocaveflag = 1;
	    timeaveflag = 0;
	    sumflag = 0;
	    break;

	case 'y':	/* print sample count */
	    countflag = 1;
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
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind >= argc)
	usage();

    archive = argv[optind];
    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n", pmProgname, archive, pmErrStr(sts));
	exit(1);
    }
    optind++;

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	    fprintf(stderr, "%s: Cannot load namespace from \"%s\": %s\n", pmProgname, pmnsfile, pmErrStr(sts));
	    exit(1);
	}
    }

    if ((sts = pmTrimNameSpace()) < 0) {
	fprintf(stderr, "%s: pmTrimNamespace failed: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
	    pmProgname, pmErrStr(sts));
	exit(1);
    }
    else
	logstart = label.ll_start;

    if (zflag) {
	if ((sts = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
	printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
	    label.ll_hostname);
    }
    else if (tz != NULL) {
	if ((sts = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmProgname, tz, pmErrStr(sts));
	    exit(1);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }

    if ((sts = pmGetArchiveEnd(&logend)) < 0) {
	logend.tv_sec = INT_MAX;
	logend.tv_usec = 0;
	fflush(stdout);
	fprintf(stderr, "%s: Cannot locate end of archive: %s\n",
	    pmProgname, pmErrStr(sts));
	fprintf(stderr, "\nWARNING: This archive is sufficiently damaged that it may not be possible to\n");
	fprintf(stderr, "         produce complete information.  Continuing and hoping for the best.\n\n");
	fflush(stderr);
    }

    if (tflag || sflag) {
	if (pmParseTimeWindow(startstr, endstr, NULL, NULL, &logstart, &logend,
		&windowstart, &windowend, &timespan, &msg) < 0) {
	    fprintf(stderr, "%s: Invalid time window specified:\n%s\n", pmProgname, msg);
	    exit(1);
	}
#ifdef PCP_DEBUG
	else if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "parseTimeWindow: \n "
		"Window start=%.0f end=%.0f sec\nArchive start=%.0f end=%.0f sec\n",
		tosec(windowstart), tosec(windowend), tosec(logstart), tosec(logend));
	    fprintf(stderr, "\nWindow Start: ");
	    __pmPrintStamp(stderr, &windowstart);
	    fprintf(stderr, "\nWindow End:   ");
	    __pmPrintStamp(stderr, &windowend);
	    fprintf(stderr, "\n\n");
	}
#endif
    }
    else {	/* time window covers whole log */
	windowstart = logstart;
	windowend = logend;
    }
    logspan = tosec(windowend) - tosec(windowstart);

    if ((sts = pmSetMode(PM_MODE_FORW, &windowstart, 0)) < 0) {
	fprintf(stderr, "%s: pmSetMode failed: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    if (lflag == 1) {
	char	       *ddmm;
	char	       *yr;

	printf("Log Label (Log Format Version %d)\n", label.ll_magic & 0xff);
	printf("Performance metrics from host %s\n", label.ll_hostname);

	ddmm = pmCtime(&windowstart.tv_sec, timebuf);
	ddmm[10] = '\0';
	yr = &ddmm[20];
	printf("  commencing %s ", ddmm);
	__pmPrintStamp(stdout, &windowstart);
	printf(" %4.4s\n", yr);

	ddmm = pmCtime(&windowend.tv_sec, timebuf);
	ddmm[10] = '\0';
	yr = &ddmm[20];
	printf("  ending     %s ", ddmm);
	__pmPrintStamp(stdout, &windowend);
	printf(" %4.4s\n", yr);
    }

    /* check which timestamp print format we should be using */
    timespan = windowend;
    tsub(&timespan, &windowstart);
    if (timespan.tv_sec > 86400) /* seconds per day: 60*60*24 */
	dayflag = 1;

    for (trip = 0; trip < 2; trip++) {	/* two passes if binning */
	for ( ; ; ) {
	    if ((sts = pmFetchArchive(&result)) < 0)
		break;

	    if (windowend.tv_sec > result->timestamp.tv_sec ||
		(windowend.tv_sec == result->timestamp.tv_sec &&
		 windowend.tv_usec >= result->timestamp.tv_usec)) {
		if (trip == 0)
		    calcaverage(result);
		else
		    calcbinning(result);
		pmFreeResult(result);
	    }
	    else {
		pmFreeResult(result);
		sts = PM_ERR_EOL;
		break;
	    }
	}

	if (trip == 0 && nbins > 0) {	/* distribute values into bins */
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "resetting for second iteration\n");
#endif
	    if ((sts = pmSetMode(PM_MODE_FORW, &windowstart, 0)) < 0) {
		fprintf(stderr, "%s: pmSetMode reset failed: %s\n",
		    pmProgname, pmErrStr(sts));
		exit(1);
	    }
	}
	else
	    break;	/* two passes only when doing binning */
    }

    if (sts != PM_ERR_EOL) {
	fprintf(stderr, "%s: fetch failed: %s\n", pmProgname, pmErrStr(sts));
	exitstatus = 1;
    }

    if (Hflag)
	printheaders();

    if (optind >= argc) {	/* print all results */
	if ((sts = pmTraversePMNS("", printsummary)) < 0) {
	    fprintf(stderr, "%s: PMNS traversal failed: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }
    else {		/* print only selected results */
	for (i = optind; i < argc; i++) {
	    char *msg;

	    if (pmParseMetricSpec(argv[i], 1, archive, &msp, &msg) < 0) {
		fputs(msg, stderr);
		free(msg);
		continue;
	    }
	    if ((sts = pmTraversePMNS(msp->metric, printsummary)) < 0)
		fprintf(stderr, "%s: PMNS traversal failed for %s: %s\n",
			pmProgname, msp->metric, pmErrStr(sts));
	    pmFreeMetricSpec(msp);
	}
    }

    exit(exitstatus);
}
