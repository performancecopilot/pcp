/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <limits.h>
#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "logcheck.h"

char		sep;
int		vflag;		/* verbose off by default */
int		index_state = STATE_MISSING;
int		meta_state = STATE_MISSING;
int		log_state = STATE_MISSING;
char		*archbasename;	/* after basename() */
char		*archdirname;	/* after dirname() */
__pmLogLabel	log_label;

typedef struct {
    int			inst;
    double		lastval;	/* value from previous sample */
    struct timeval	lasttime;	/* time of previous sample */
} instData;

typedef struct {
    pmDesc		desc;
    double		scale;
    instData		**instlist;
    unsigned int	listsize;
} checkData;

static __pmHashCtl	hashlist;	/* hash statistics about each metric */
static int		dayflag;
static char		timebuf[32];	/* for pmCtime result + .xxx */
static char		*archpathname;	/* from the command line */

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "label", 0, 'l', 0, "print the archive label" },
    PMOPT_NAMESPACE,
    PMOPT_START,
    PMOPT_FINISH,
    { "verbose", 0, 'l', 0, "verbose output" },
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_DONE | PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
    .short_options = "D:ln:S:T:zvZ:?",
    .long_options = longopts,
    .short_usage = "[options] archive",
};

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
	fprintf(stderr, "[");
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
	fprintf(stderr, "[");
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

	fprintf(stderr, "[");
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
		fprintf(stderr, "[");
		print_stamp(stderr, &result->timestamp);
		fprintf(stderr, "] ");
		print_metric(stderr, vsp->pmid);
		fprintf(stderr, ": no values returned\n");
		continue;
	    }
	    else if (vsp->numval < 0) {
		fprintf(stderr, "[");
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
		fprintf(stderr, "[");
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
	    if (__pmHashAdd(checkdata->desc.pmid, (void*)checkdata, &hashlist) < 0) {
		fprintf(stderr, "[");
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
		    fprintf(stderr, "[");
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
			fprintf(stderr, "[");
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

static void
dumpLabel(void)
{
    pmLogLabel	label;
    char	*ddmm;
    char	*yr;
    int		sts;

    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Log Label (Log Format Version %d)\n", label.ll_magic & 0xff);
    fprintf(stderr, "Performance metrics from host %s\n", label.ll_hostname);

    ddmm = pmCtime(&label.ll_start.tv_sec, timebuf);
    ddmm[10] = '\0';
    yr = &ddmm[20];
    fprintf(stderr, "  commencing %s ", ddmm);
    __pmPrintStamp(stderr, &label.ll_start);
    fprintf(stderr, " %4.4s\n", yr);

    if (opts.finish.tv_sec == INT_MAX) {
        /* pmGetArchiveEnd() failed! */
        fprintf(stderr, "  ending     UNKNOWN\n");
    }
    else {
        ddmm = pmCtime(&opts.finish.tv_sec, timebuf);
        ddmm[10] = '\0';
        yr = &ddmm[20];
        fprintf(stderr, "  ending     %s ", ddmm);
        __pmPrintStamp(stderr, &opts.finish);
        fprintf(stderr, " %4.4s\n", yr);
    }
}

static int
filter(const_dirent *dp)
{
    static int	len = -1;

    if (len == -1)
	len = strlen(archbasename);
    if (strncmp(dp->d_name, archbasename, len) != 0)
	return 0;
    if (strcmp(&dp->d_name[len], ".meta") == 0)
	return 1;
    if (strcmp(&dp->d_name[len], ".index") == 0)
	return 1;
    if (dp->d_name[len] == '.' && isdigit(dp->d_name[len+1])) {
	const char	*p = &dp->d_name[len+2];
	for ( ; *p; p++) {
	    if (!isdigit(*p))
		return 0;
	}
	return 1;
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    int			c;
    int			sts;
    int			ctx;
    int			i;
    int			lflag = 0;	/* no label by default */
    int			nfile;
    int			mark_count = 0;
    int			result_count = 0;
    int			n;
    char		*p;
    struct dirent	**namelist;
    __pmContext		*ctxp;
    pmResult		*result;
    struct timeval	timespan;
    struct timeval	last_stamp;
    struct timeval	delta_stamp;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'l':	/* display the archive label */
	    lflag = 1;
	    break;
	case 'v':	/* bump verbosity */
	    vflag++;
	    break;
	}
    }

    if (!opts.errors && opts.optind >= argc) {
	pmprintf("Error: no archive specified\n\n");
	opts.errors++;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    sep = __pmPathSeparator();
    setlinebuf(stderr);

    __pmAddOptArchive(&opts, argv[opts.optind]);
    opts.flags &= ~PM_OPTFLAG_DONE;
    __pmEndOptions(&opts);

    archpathname = argv[opts.optind];
    archbasename = strdup(basename(strdup(archpathname)));
    /*
     * treat foo, foo.index, foo.meta, foo.NNN as all equivalent
     * to "foo"
     */
    p = strrchr(archbasename, '.');
    if (p != NULL) {
	if (strcmp(p, ".index") == 0 || strcmp(p, ".meta") == 0)
	    *p = '\0';
	else {
	    char	*q = ++p;
	    if (isdigit(*q)) {
		q++;
		while (*q && isdigit(*q))
		    q++;
		if (*q == '\0')
		    *p = '\0';
	    }
	}
    }
    archdirname = dirname(strdup(archpathname));
    if (vflag)
	fprintf(stderr, "Scanning for components of archive \"%s\"\n", archpathname);
    nfile = scandir(archdirname, &namelist, filter, NULL);
    if (nfile < 1) {
	fprintf(stderr, "%s: no PCP archive files match \"%s\"\n", pmProgname, archpathname);
	exit(EXIT_FAILURE);
    }

    /*
     * Pass 0 for data, metadata and index files ... check physical
     * archive record structure, then label record
     */
    sts = STS_OK;
    for (i = 0; i < nfile; i++) {
	char	path[MAXPATHLEN];
	if (strcmp(archdirname, ".") == 0) {
	    /* skip ./ prefix */
	    strncpy(path, namelist[i]->d_name, sizeof(path));
	}
	else {
	    snprintf(path, sizeof(path), "%s%c%s", archdirname, sep, namelist[i]->d_name);
	}
	if (pass0(path) == STS_FATAL)
	    /* unrepairable or unrepaired error */
	    sts = STS_FATAL;
    }
    if (meta_state == STATE_MISSING) {
	fprintf(stderr, "%s%c%s.meta: missing metadata file\n", archdirname, sep, archbasename);
	sts = STS_FATAL;
    }
    if (log_state == STATE_MISSING) {
	fprintf(stderr, "%s%c%s.0 (or similar): missing log file \n", archdirname, sep, archbasename);
	sts = STS_FATAL;
    }

    if (sts == STS_FATAL) {
	if (vflag) fprintf(stderr, "Due to earlier errors, cannot continue ... bye\n");
	exit(EXIT_FAILURE);
    }

    if ((sts = ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archpathname)) < 0) {
	fprintf(stderr, "%s: cannot open archive \"%s\": %s\n", pmProgname, archpathname, pmErrStr(sts));
	fprintf(stderr, "Checking abandoned.\n");
	exit(EXIT_FAILURE);
    }

    if (pmGetContextOptions(ctx, &opts) < 0) {
        pmflush();      /* runtime errors only at this stage */
        exit(EXIT_FAILURE);
    }

    if (lflag)
	dumpLabel();

    /*
     * Note: ctxp->c_lock remains locked throughout ... __pmHandleToPtr()
     *       is only called once, and a single context is used throughout
     *       ... so there is no PM_UNLOCK(ctxp->c_lock) anywhere in the
     *       pmchecklog code.
     *       This works because ctxp->c_lock is a recursive lock and
     *       pmchecklog is single-threaded.
     */
    if ((n = pmWhichContext()) >= 0) {
	if ((ctxp = __pmHandleToPtr(n)) == NULL) {
	    fprintf(stderr, "%s: botch: __pmHandleToPtr(%d) returns NULL!\n", pmProgname, n);
	    exit(EXIT_FAILURE);
	}
    }
    else {
	fprintf(stderr, "%s: botch: %s!\n", pmProgname, pmErrStr(PM_ERR_NOCONTEXT));
	exit(EXIT_FAILURE);
    }

    pass1(ctxp, archpathname);

    if (index_state == STATE_BAD) {
	/* prevent subsequent use of bad temporal index */
	ctxp->c_archctl->ac_log->l_numti = 0;
    }

    if ((sts = pmSetMode(PM_MODE_FORW, &opts.start, 0)) < 0) {
	fprintf(stderr, "%s: pmSetMode failed: %s\n", pmProgname, pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    /* check which timestamp print format we should be using */
    timespan = opts.finish;
    tsub(&timespan, &opts.start);
    if (timespan.tv_sec > 86400) /* seconds per day: 60*60*24 */
	dayflag = 1;

    sts = 0;
    last_stamp = opts.start;
    for ( ; ; ) {
	if ((sts = pmFetchArchive(&result)) < 0)
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

	    fprintf(stderr, "[");
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
	    fprintf(stderr, "[");
	    print_stamp(stderr, &result->timestamp);
	    fprintf(stderr, "]: timestamp went backwards, prior timestamp: ");
	    print_stamp(stderr, &last_stamp);
	    fprintf(stderr, "\n");
	}

	last_stamp = result->timestamp;
	if ((opts.finish.tv_sec > result->timestamp.tv_sec) ||
	    ((opts.finish.tv_sec == result->timestamp.tv_sec) &&
	     (opts.finish.tv_usec >= result->timestamp.tv_usec))) {
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
	exit(EXIT_FAILURE);
    }

    if (vflag) {
	if (result_count > 0)
	    fprintf(stderr, "Processed %d pmResult records\n", result_count);
	if (mark_count > 0)
	    fprintf(stderr, "Processed %d <mark> records\n", mark_count);
    }

    return 0;
}
