/*
 * Copyright (c) 2013-2014 Red Hat.
 * Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmtime.h"
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include <ctype.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

struct statsrc_t {
    int		ctx;
    int		flip;
    char *	sname;
    pmID *	pmids;
    pmDesc *	pmdesc;
    pmResult *	res[2];
};

static char * metrics[] = {
#define LOADAVG 0
    "kernel.all.load",   
#define MEM 1
    "swap.used",            
    "mem.util.free",        
    "mem.util.bufmem",      
    "mem.util.cached",      
#define SWAP 5
    "swap.pagesin",              
    "swap.pagesout",             
#define IO 7
    "disk.all.blkread",     
    "disk.all.blkwrite",    
#define SYSTEM 9
    "kernel.all.intr",      
    "kernel.all.pswitch",   
#define CPU 11
    "kernel.all.cpu.nice",  
    "kernel.all.cpu.user",  
    "kernel.all.cpu.intr",
    "kernel.all.cpu.sys",
    "kernel.all.cpu.idle",
    "kernel.all.cpu.wait.total",
    "kernel.all.cpu.steal",
};

static char * metricSubst[] = {
    NULL,
/*Memory*/
    NULL,
    NULL,
    "mem.bufmem",
    NULL,
/*Swap*/
    "swap.in",              
    "swap.out",             
/*IO*/
    "disk.all.read",
    "disk.all.write",
/*System*/
    NULL,
    NULL,
/*CPU*/
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static const int nummetrics = sizeof(metrics)/sizeof (metrics[0]);

pmLongOptions longopts[] = {
    PMAPI_GENERAL_OPTIONS,
    PMAPI_OPTIONS_HEADER("Alternate sources"),
    PMOPT_HOSTSFILE,
    PMOPT_LOCALPMDA,
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "suffix", 0, 'l', 0, "print last 7 charcters of the host name(s)" },
    { "pause", 0, 'P', 0, "pause between updates for archive replay" },
    { "xcpu", 0, 'x', 0, "extended CPU statistics reporting" },
    PMAPI_OPTIONS_END
};

pmOptions opts = {
    .flags = PM_OPTFLAG_MULTI | PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
    .short_options = PMAPI_OPTIONS "H:LlPx",
    .long_options = longopts,
};

static int extraCpuStats;
static char swapOp = 'p';
static int rows = 21;
static int header;
static float period;
static pmTimeControls defaultcontrols;


static struct statsrc_t *
getNewContext(int type, char * host, int quiet)
{
    struct statsrc_t *s;

    if ((s = (struct statsrc_t *)malloc(sizeof (struct statsrc_t))) != NULL) {
	if ((s->ctx = pmNewContext(type, host)) < 0 ) {
	    if (!quiet)
		fprintf(stderr,
			"%s: Cannot create context to get data from %s: %s\n",
			pmProgname, host, pmErrStr(s->ctx));
	    free(s);
	    s = NULL;
	} else {
	    int sts;
	    int i;
	    
	    if ((s->pmids = calloc(nummetrics, sizeof(pmID))) == NULL) {
		free(s);
		return NULL;
	    }

	    if ((sts = pmLookupName(nummetrics, metrics, s->pmids)) != nummetrics) {
		if (sts >= 0) {
		    for (i = 0; i < nummetrics; i++) {
			if (s->pmids[i] != PM_ID_NULL)
			    continue;
			if (metricSubst[i] == NULL) {
			    /* skip these, as archives may not contain 'em */
			    if (i != CPU+2 && i != CPU+5 && i != CPU+6) {
				int e2 = pmLookupName(1,metrics+i, s->pmids+i);
				if (e2 != 1)
				    fprintf(stderr, 
					"%s: %s: no metric \"%s\": %s\n",
					pmProgname, host, metrics[i],
					pmErrStr(e2));
			    }
			} else {
			    int e2 = pmLookupName(1,metricSubst+i, s->pmids+i);
			    if (e2 != 1) {
				fprintf (stderr,
					 "%s: %s: no metric \"%s\" nor \"%s\": %s\n",
					 pmProgname, host, metrics[i], 
					 metricSubst[i], pmErrStr(e2));
			    }
			    else {
				fprintf (stderr,
					 "%s: %s: Warning: using metric \"%s\" instead of \"%s\"\n",
					 pmProgname, host,
					 metricSubst[i], metrics[i]);
				if (i == SWAP || i == SWAP+1)
				    swapOp = 's';
			    }
			}
		    }
		}
		else {
		    fprintf(stderr, "%s: pmLookupName: %s\n",
			    pmProgname, pmErrStr(sts));
		    free(s->pmids);
		    free(s);
		    return NULL;
		}
	    }
	    
	    if ((s->pmdesc = calloc(nummetrics, sizeof (pmDesc))) == NULL) {
		free(s->pmids);
		free(s);
		return NULL;
	    }

	    for (i = 0; i < nummetrics; i++) {
		if (s->pmids[i] == PM_ID_NULL) {
		    s->pmdesc[i].indom = PM_INDOM_NULL;
		    s->pmdesc[i].pmid = PM_ID_NULL;
		} else {
		    if ((sts = pmLookupDesc(s->pmids[i], s->pmdesc+i)) < 0) {
			fprintf(stderr, 
				"%s: %s: Warning: cannot retrieve description for "
				"metric \"%s\" (PMID: %s)\nReason: %s\n",
				pmProgname, host, metrics[i], pmIDStr(s->pmids[i]),
				pmErrStr(sts));
			s->pmdesc[i].indom = PM_INDOM_NULL;
			s->pmdesc[i].pmid = PM_ID_NULL;
			
		    }
		}
	    }

	    s->flip = 0;
	    s->sname = NULL;
	    s->res[0] = s->res[1] = NULL;
	}
    }

    return s;
}

static char *
saveContextHostName(int ctx)
{
    char	hostname[MAXHOSTNAMELEN];
    char	*name = pmGetContextHostName_r(ctx, hostname, sizeof(hostname));
    size_t	length;

    if ((length = strlen(name)) == 0)
	fprintf(stderr, "%s: Warning: pmGetContextHostName(%d) failed\n",
		pmProgname, ctx);
    if ((name = strdup(name)) == NULL)
	__pmNoMem("context name", length + 1, PM_FATAL_ERR);
    return name;
}

static void
destroyContext(struct statsrc_t *s)
{
    if (s != NULL && s->ctx >= 0) {
	int	index;

	if (!s->sname)
	    s->sname = saveContextHostName(s->ctx);
	pmDestroyContext(s->ctx);
	s->ctx = -1;
	free(s->pmdesc);
	s->pmdesc = NULL;
	free(s->pmids);
	s->pmids = NULL;
	index = 1 - s->flip;
	if (s->res[index] != NULL)
	    pmFreeResult(s->res[index]);
	s->res[index] = NULL;
    }
}

static long long
countDiff(pmDesc *d, pmValueSet *now, pmValueSet *was)
{
    long long diff = 0;
    pmAtomValue a;
    pmAtomValue b;

    pmExtractValue(was->valfmt, &was->vlist[0], d->type, &a, d->type);
    pmExtractValue(now->valfmt, &now->vlist[0], d->type, &b, d->type);
    switch (d->type) {
    case PM_TYPE_32:
	diff = b.l - a.l;
	break;
    case PM_TYPE_U32:
	diff = b.ul - a.ul;
	break;
    case PM_TYPE_U64:
	diff = b.ull - a.ull;
	break;
    }
    return diff;
}

static void
scalePrint(long value)
{
    if (value < 10000)
	printf (" %4ld", value);
    else {
	value /= 1000;	/* '000s */
	if (value < 1000)
	    printf(" %3ldK", value);
	else {
	    value /= 1000;	/* '000,000s */
	    printf(" %3ldM", value);
	}
    }
}

static void
timeinterval(struct timeval delta)
{
    defaultcontrols.interval(delta);
    opts.interval = delta;
    period = (delta.tv_sec * 1.0e6 + delta.tv_usec) / 1e6;
    header = 1;
}

static void
timeresumed(void)
{
    defaultcontrols.resume();
    header = 1;
}

static void
timerewind(void)
{
    defaultcontrols.rewind();
    header = 1;
}

static void
timenewzone(char *tz, char *label)
{
    defaultcontrols.newzone(tz, label);
    header = 1;
}

static void
timeposition(struct timeval position)
{
    defaultcontrols.position(position);
    header = 1;
}

#if defined(TIOCGWINSZ)
static void
resize(int sig)
{
    struct winsize win;

    if (ioctl(1, TIOCGWINSZ, &win) != -1 && win.ws_row > 0)
	rows = win.ws_row - 3;
}
#endif

static int
setupTimeOptions(int ctx, pmOptions *opts, char **tzlabel)
{
    char *label = (char *)pmGetContextHostName(ctx);
    char *zone;

    if (pmGetContextOptions(ctx, opts)) {
	pmflush();
	exit(1);
    }
    if (opts->timezone)
	*tzlabel = opts->timezone;
    else
	*tzlabel = label;
    return pmWhichZone(&zone);
}

int
main(int argc, char *argv[]) 
{
    time_t now;
    pmTime *pmtime = NULL;
    pmTimeControls controls;
    struct statsrc_t *pd;
    struct statsrc_t **ctxList = &pd;
    int ctxCount = 0;
    int sts, j;
    int iteration;
    int pauseFlag = 0;
    int printTail = 0;
    int tzh = -1;
    char *tzlabel = NULL;
    char **nameList;
    int nameCount;

    setlinebuf(stdout);

    while ((sts = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (sts) {
	case 'l':	/* print last 7 characters of hostname(s) */
	    printTail++;
	    break;
	case 'P':	/* pause between updates when replaying an archive */
	    pauseFlag++;
	    break;
	case 'x':	/* extended CPU reporting */
	    extraCpuStats = 1;
	    break;
	default:
	    opts.errors++;
	    break;
	}
    }

    if (argc != opts.optind) {
	pmprintf("%s: too many options\n", pmProgname);
	opts.errors++;
    }

    if (pauseFlag && (opts.context != PM_CONTEXT_ARCHIVE)) {
	pmprintf("%s: -P can only be used with archives\n", pmProgname);
	opts.errors++;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if (opts.interval.tv_sec == 0 && opts.interval.tv_usec == 0)
	opts.interval.tv_sec = 5;	/* 5 sec default sampling */

    if (opts.context == PM_CONTEXT_ARCHIVE) {
	nameCount = opts.narchives;
	nameList = opts.archives;
    } else {
	if (opts.context == 0)
	    opts.context = PM_CONTEXT_HOST;
	nameCount = opts.nhosts;
	nameList = opts.hosts;
    }

    if (nameCount) {
	if ((ctxList = calloc(nameCount, sizeof(struct statsrc_t *))) != NULL) {
	    int ct;

	    for (ct = 0; ct < nameCount; ct++) {
		if ((pd = getNewContext(opts.context, nameList[ct], 0)) != NULL) {
		    ctxList[ctxCount++] = pd;
		    if (tzh < 0)
			tzh = setupTimeOptions(pd->ctx, &opts, &tzlabel);
		    pmUseZone(tzh);
		}
	    }
	} else {
	    __pmNoMem("contexts", nameCount * sizeof(struct statsrc_t *), PM_FATAL_ERR);
	}
    } else {
	/*
	 * Read metrics from the local host.  Note that context can be 
	 * either LOCAL or HOST, but not ARCHIVE here.  If we fail to
	 * talk to pmcd we fallback to local context mode automagically.
	 */
	if ((pd = getNewContext(opts.context, "local:", 1)) == NULL) {
	    opts.context = PM_CONTEXT_LOCAL;
	    pd = getNewContext(opts.context, NULL, 0);
	}
	if (pd) {
	    tzh = setupTimeOptions(pd->ctx, &opts, &tzlabel);
	    ctxCount = 1;
	}
    }

    if (!ctxCount) {
	fprintf(stderr, "%s: No place to get data from!\n", pmProgname);
	exit(1);
    }

#if defined(TIOCGWINSZ)
# if defined(SIGWINCH)
    __pmSetSignalHandler(SIGWINCH, resize);
# endif
    resize(0);
#endif

    /* calculate the number of samples needed, if given an end time */
    period = (opts.interval.tv_sec * 1.0e6 + opts.interval.tv_usec) / 1e6;
    now = (time_t)(opts.start.tv_sec + 0.5 + opts.start.tv_usec / 1.0e6);
    if (opts.finish_optarg) {
	double win = opts.finish.tv_sec - opts.origin.tv_sec + 
		    (opts.finish.tv_usec - opts.origin.tv_usec) / 1e6;
	win /= period;
	if (win > opts.samples)
	    opts.samples = (int)win;
    }

    if (opts.guiflag != 0 || opts.guiport != 0) {
	char *timezone;

	pmWhichZone(&timezone);
	if (!opts.guiport)
	    opts.guiport = -1;
	pmtime = pmTimeStateSetup(&controls, opts.context,
			opts.guiport, opts.interval, opts.origin,
			opts.start, opts.finish, timezone, tzlabel);

	/* keep pointers to some default time control functions */
	defaultcontrols = controls;

	/* custom time control routines */
	controls.rewind = timerewind;
	controls.resume = timeresumed;
	controls.newzone = timenewzone;
	controls.interval = timeinterval;
	controls.position = timeposition;
	opts.guiflag = 1;
    }

    /* Do first fetch */
    for (j = 0; j < ctxCount; j++) {
	struct statsrc_t *pd = ctxList[j];

	pmUseContext(pd->ctx);

	if (!opts.guiflag && opts.context == PM_CONTEXT_ARCHIVE)
	    pmTimeStateMode(PM_MODE_INTERP, opts.interval, &opts.origin);

	if (pd->ctx >= 0) {
	    if ((sts = pmFetch(nummetrics, pd->pmids, pd->res + pd->flip)) < 0)
		pd->res[pd->flip] = NULL;
	    else
		pd->flip = 1 - pd->flip;
	}
    }

    for (iteration = 0; !opts.samples || iteration < opts.samples; iteration++) {
	if ((iteration * ctxCount) % rows < ctxCount)
	    header = 1;

	if (header) {
	    pmResult *r = ctxList[0]->res[1 - ctxList[0]->flip];
	    char tbuf[26];

	    if (r != NULL)
		now = (time_t)(r->timestamp.tv_sec + 0.5 + 
			       r->timestamp.tv_usec/ 1.0e6);
	    printf("@ %s", pmCtime(&now, tbuf));

	    if (ctxCount > 1) {
		printf("%-7s%8s%21s%10s%10s%10s%*s\n",
			"node", "loadavg","memory","swap","io","system",
			extraCpuStats ? 20 : 12, "cpu");
		if (extraCpuStats)
		    printf("%8s%7s %6s %6s %6s   %c%1s   %c%1s %4s %4s %4s %4s %3s %3s %3s %3s %3s\n",
			"", "1 min","swpd","buff","cache", swapOp,"i",swapOp,"o","bi","bo",
			"in","cs","us","sy","id","wa","st");
		else
		    printf("%8s%7s %6s %6s %6s   %c%1s   %c%1s %4s %4s %4s %4s %3s %3s %3s\n",
			"", "1 min","swpd","buff","cache", swapOp,"i",swapOp,"o","bi","bo",
			"in","cs","us","sy","id");

	    } else {
		printf("%8s%28s%10s%10s%10s%*s\n",
		       "loadavg","memory","swap","io","system",
			extraCpuStats ? 20 : 12, "cpu");
		if (extraCpuStats)
		    printf(" %7s %6s %6s %6s %6s   %c%1s   %c%1s %4s %4s %4s %4s %3s %3s %3s %3s %3s\n",
			"1 min","swpd","free","buff","cache", swapOp,"i",swapOp,"o","bi","bo",
			"in","cs","us","sy","id","wa","st");
		else
		    printf(" %7s %6s %6s %6s %6s   %c%1s   %c%1s %4s %4s %4s %4s %3s %3s %3s\n",
			"1 min","swpd","free","buff","cache", swapOp,"i",swapOp,"o","bi","bo",
			"in","cs","us","sy","id");
	    }
	    header = 0;
	}

	if (opts.guiflag)
	    pmTimeStateVector(&controls, pmtime);
	else if (opts.context != PM_CONTEXT_ARCHIVE || pauseFlag)
	    __pmtimevalSleep(opts.interval);
	if (header)
	    goto next;

	for (j = 0; j < ctxCount; j++) {
	    int i;
	    unsigned long long dtot = 0;
	    unsigned long long diffs[7];
	    pmAtomValue la;
	    struct statsrc_t *s = ctxList[j];

	    if (ctxCount > 1) {
		const char *fn;

		if (!s->sname)
		    s->sname = saveContextHostName(s->ctx);
		fn = s->sname;

		if (printTail)
		    printf("%-7s", strlen(fn) <= 7 ? fn : fn + strlen(fn) - 7);
		else
		    printf("%-7.7s", fn);

		if (s->ctx < 0) {
		    putchar('\n');
		    continue;
		}

		pmUseContext(s->ctx);
	    }

	    if ((sts = pmFetch(nummetrics, s->pmids, s->res + s->flip)) < 0) {
		if (opts.context == PM_CONTEXT_HOST &&
		    (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)) {
		    puts(" Fetch failed. Reconnecting ...");
		    i = 1 - s->flip;
		    if (s->res[i] != NULL) {
			pmFreeResult(s->res[i]);
			s->res[i] = NULL;
		    }
		    pmReconnectContext(s->ctx);
		} else if ((opts.context == PM_CONTEXT_ARCHIVE) && 
			   (sts == PM_ERR_EOL) && opts.guiflag) {
		    pmTimeStateBounds(&controls, pmtime);
		} else if ((opts.context == PM_CONTEXT_ARCHIVE) && 
			   (sts == PM_ERR_EOL) &&
			   (s->res[0] == NULL) && (s->res[1] == NULL)) {
		    /* I'm yet to see something from this archive - don't
		     * discard it just yet */
		    puts(" No data in the archive");
		} else {
		    int k;
		    int valid = 0;

		    printf(" pmFetch: %s\n", pmErrStr(sts));

		    destroyContext(s);
		    for (k = 0; k < ctxCount; k++)
			valid += (ctxList[k]->ctx >= 0);
		    if (!valid)
			exit(1);
		}
	    } else {
		pmResult *cur = s->res[s->flip];
		pmResult *prev = s->res[1 - s->flip];


		/* LoadAvg - Assume that 1min is the first one */
		if (s->pmdesc[LOADAVG].pmid == PM_ID_NULL ||
		    cur->vset[LOADAVG]->numval < 1) 
		    printf(" %7.7s", "?");
		else {
		    pmExtractValue(cur->vset[LOADAVG]->valfmt,
				   &cur->vset[LOADAVG]->vlist[0], 
				   s->pmdesc[LOADAVG].type,
				   &la, PM_TYPE_FLOAT);
		    
		    printf(" %7.2f", la.f);
		}
	    
		/* Memory state */
		for (i = 0; i < 4; i++) {
		    if (i == 2 && ctxCount > 1) 
			continue; /* Don't report free mem for multiple hosts */

		    if (cur->vset[MEM+i]->numval == 1) {
			pmUnits kb = PMDA_PMUNITS(1, 0, 0, 
						  PM_SPACE_KBYTE, 0, 0);

			pmExtractValue(cur->vset[MEM+i]->valfmt,
				       &cur->vset[MEM+i]->vlist[0], 
				       s->pmdesc[MEM+i].type,
				       &la, PM_TYPE_U32);
			pmConvScale(s->pmdesc[MEM+i].type, & la, 
				     & s->pmdesc[MEM+i].units, &la, &kb);

			if (la.ul < 1000000)
			    printf(" %6u", la.ul);
			else {
			    la.ul /= 1024;	/* PM_SPACE_MBYTE now */
			    if (la.ul < 100000)
				printf(" %5um", la.ul);
			    else {
				la.ul /= 1024;      /* PM_SPACE_GBYTE now */
				printf(" %5ug", la.ul);
			    }
			}
		    } else 
			printf(" %6.6s", "?");
		}

		/* Swap in/out */
		for (i = 0; i < 2; i++) {
		    if (s->pmdesc[SWAP+i].pmid == PM_ID_NULL || prev == NULL ||
			prev->vset[SWAP+i]->numval != 1 ||
			cur->vset[SWAP+i]->numval != 1) 
			printf(" %4.4s", "?");
		    else
			scalePrint(countDiff(s->pmdesc+SWAP+i, cur->vset[SWAP+i], prev->vset[SWAP+i])/period);
		}

		/* io in/out */
		for (i = 0; i < 2; i++) {
		    if (s->pmdesc[IO+i].pmid == PM_ID_NULL || prev == NULL ||
			prev->vset[IO+i]->numval != 1 ||
			cur->vset[IO+i]->numval != 1) 
			printf(" %4.4s", "?");
		    else 
			scalePrint(countDiff(s->pmdesc+IO+i, cur->vset[IO+i], prev->vset[IO+i])/period);
		}

		/* system interrupts */
		for (i = 0; i < 2; i++) {
		    if (s->pmdesc[SYSTEM+i].pmid == PM_ID_NULL || 
			prev == NULL ||
			prev->vset[SYSTEM+i]->numval != 1 ||
			cur->vset[SYSTEM+i]->numval != 1) 
			printf(" %4.4s", "?");
		    else
			scalePrint(countDiff(s->pmdesc+SYSTEM+i, cur->vset[SYSTEM+i], prev->vset[SYSTEM+i])/period);
		}

		/* CPU utilization - report percentage */
		for (i = 0; i < 7; i++) {
		    if (s->pmdesc[CPU+i].pmid == PM_ID_NULL || prev == NULL ||
			cur->vset[CPU+i]->numval != 1 ||
			prev->vset[CPU+i]->numval != 1) {
			if (i > 0 && i < 4 && i != 2) {
			    break;
			} else { /* Nice, intr, iowait, steal are optional */
			    diffs[i] = 0;
			}
		    } else {
			diffs[i] = countDiff(s->pmdesc + CPU+i,
					cur->vset[CPU+i], prev->vset[CPU+i]);
			dtot += diffs[i];
		    }
		}

		if (extraCpuStats) {
		    if (i != 7 || dtot == 0) {
			printf(" %3.3s %3.3s %3.3s %3.3s %3.3s",
				"?", "?", "?", "?", "?");
		    } else {
			unsigned long long fill = dtot/2;
			printf(" %3u %3u %3u %3u %3u",
			   (unsigned int)((100*(diffs[0]+diffs[1])+fill)/dtot),
			   (unsigned int)((100*(diffs[2]+diffs[3])+fill)/dtot),
			   (unsigned int)((100*diffs[4]+fill)/dtot),
			   (unsigned int)((100*diffs[5]+fill)/dtot),
			   (unsigned int)((100*diffs[6]+fill)/dtot));
		    }
		} else if (i != 7 || dtot == 0) {
		    printf(" %3.3s %3.3s %3.3s", "?", "?", "?");
		} else {
		    unsigned long long fill = dtot/2;
		    printf(" %3u %3u %3u",
			   (unsigned int)((100*(diffs[0]+diffs[1])+fill)/dtot),
			   (unsigned int)((100*(diffs[2]+diffs[3])+fill)/dtot),
			   (unsigned int)((100*diffs[4]+fill)/dtot));
		}

		if (prev != NULL)
		    pmFreeResult(prev);
		s->flip = 1 - s->flip;
		s->res[s->flip] = NULL;

		putchar('\n');
	    }
	}

next:
	if (opts.guiflag)
	    pmTimeStateAck(&controls, pmtime);

	now += (time_t)period;
    }

    exit(EXIT_SUCCESS);
}
