/*
 * Copyright (c) 2013-2015 Red Hat.
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
/* prefer <termios.h> to the deprecated <sys/termios.h> */
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#endif
#include <assert.h>


struct statsrc_t
{
    int ctx;
    pmFG pmfg;
    char *sname;
    struct timeval timestamp;
    int fetched_p;
    pmAtomValue load_average;
    int load_average_sts;
    pmAtomValue swap_used;
    int swap_used_sts;
    pmAtomValue mem_free;
    int mem_free_sts;
    pmAtomValue mem_buf;
    int mem_buf_sts;
    pmAtomValue mem_cached;
    int mem_cached_sts;
    pmAtomValue swap_in;
    int swap_in_sts;
    pmAtomValue swap_out;
    int swap_out_sts;
    pmAtomValue blkread;
    int blkread_sts;
    pmAtomValue blkwrite;
    int blkwrite_sts;
    pmAtomValue kernel_intr;
    int kernel_intr_sts;
    pmAtomValue kernel_pswitch;
    int kernel_pswitch_sts;
    pmAtomValue kernel_cpunice;
    int kernel_cpunice_sts;
    pmAtomValue kernel_cpuuser;
    int kernel_cpuuser_sts;
    pmAtomValue kernel_cpuintr;
    int kernel_cpuintr_sts;
    pmAtomValue kernel_cpusys;
    int kernel_cpusys_sts;
    pmAtomValue kernel_cpuidle;
    int kernel_cpuidle_sts;
    pmAtomValue kernel_cpuwait;
    int kernel_cpuwait_sts;
    pmAtomValue kernel_cpusteal;
    int kernel_cpusteal_sts;
};


pmLongOptions longopts[] = {
    PMAPI_GENERAL_OPTIONS,
    PMAPI_OPTIONS_HEADER("Alternate sources"),
    PMOPT_HOSTSFILE,
    PMOPT_LOCALPMDA,
    PMAPI_OPTIONS_HEADER("Reporting options"),
    {"suffix", 0, 'l', 0, "print last 7 charcters of the host name(s)"},
    {"pause", 0, 'P', 0, "pause between updates for archive replay"},
    {"xcpu", 0, 'x', 0, "extended CPU statistics reporting"},
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
getNewContext(int type, char *host, int quiet)
{
    struct statsrc_t *s;
    int sts;

    s = calloc(1, sizeof(struct statsrc_t));
    if (s == NULL)
	goto out;

    s->ctx = pmNewContext(type, host);
    if (s->ctx < 0) {
	if (!quiet)
	    fprintf(stderr, "%s: Cannot create context to get data from %s: %s\n", pmProgname, host, pmErrStr(s->ctx));
	goto out1;
    }

    sts = pmCreateFetchGroup(&s->pmfg);
    if (sts < 0) {
	fprintf(stderr, "%s: Cannot create fetchgroup: %s\n", pmProgname, pmErrStr(sts));
	goto out2;
    }

    /* Register all metrics with desired conversions/types; some with fallbacks. */
    (void) pmExtendFetchGroup_timestamp(s->pmfg, &s->timestamp);
    s->load_average_sts =
	pmExtendFetchGroup_item(s->pmfg, "kernel.all.load", "1 minute", NULL, &s->load_average, PM_TYPE_FLOAT,
				&s->load_average_sts);
    s->swap_used_sts =
	pmExtendFetchGroup_item(s->pmfg, "swap.used", NULL, "kbyte", &s->swap_used, PM_TYPE_U32, &s->swap_used_sts);
    s->mem_free_sts =
	pmExtendFetchGroup_item(s->pmfg, "mem.util.free", NULL, "kbyte", &s->mem_free, PM_TYPE_U32, &s->mem_free_sts);
    s->mem_buf_sts =
	pmExtendFetchGroup_item(s->pmfg, "mem.util.bufmem", NULL, "kbyte", &s->mem_buf, PM_TYPE_U32, &s->mem_buf_sts);
    if (s->mem_buf_sts < 0)
	s->mem_buf_sts =
	    pmExtendFetchGroup_item(s->pmfg, "mem.bufmem", NULL, "kbyte", &s->mem_buf, PM_TYPE_U32, &s->mem_buf_sts);
    s->mem_cached_sts =
	pmExtendFetchGroup_item(s->pmfg, "mem.util.cached", NULL, "kbyte", &s->mem_cached, PM_TYPE_U32,
				&s->mem_cached_sts);
    s->swap_in_sts =
	pmExtendFetchGroup_item(s->pmfg, "swap.pagesin", NULL, NULL, &s->swap_in, PM_TYPE_U32, &s->swap_in_sts);
    if (s->swap_in_sts < 0) {
	swapOp = 's';
	s->swap_in_sts =
	    pmExtendFetchGroup_item(s->pmfg, "swap.in", NULL, NULL, &s->swap_in, PM_TYPE_U32, &s->swap_in_sts);
    }
    s->swap_out_sts =
	pmExtendFetchGroup_item(s->pmfg, "swap.pagesout", NULL, NULL, &s->swap_out, PM_TYPE_U32, &s->swap_out_sts);
    if (s->swap_out_sts < 0) {
	swapOp = 's';
	s->swap_out_sts =
	    pmExtendFetchGroup_item(s->pmfg, "swap.out", NULL, NULL, &s->swap_out, PM_TYPE_U32, &s->swap_out_sts);
    }
    s->blkread_sts =
	pmExtendFetchGroup_item(s->pmfg, "disk.all.blkread", NULL, NULL, &s->blkread, PM_TYPE_U32, &s->blkread_sts);
    if (s->blkread_sts < 0)
	s->blkread_sts =
	    pmExtendFetchGroup_item(s->pmfg, "disk.all.read", NULL, NULL, &s->blkread, PM_TYPE_U32, &s->blkread_sts);
    s->blkwrite_sts =
	pmExtendFetchGroup_item(s->pmfg, "disk.all.blkwrite", NULL, NULL, &s->blkwrite, PM_TYPE_U32, &s->blkwrite_sts);
    if (s->blkwrite_sts < 0)
	s->blkwrite_sts =
	    pmExtendFetchGroup_item(s->pmfg, "disk.all.write", NULL, NULL, &s->blkwrite, PM_TYPE_U32, &s->blkwrite_sts);
    s->kernel_intr_sts =
	pmExtendFetchGroup_item(s->pmfg, "kernel.all.intr", NULL, NULL, &s->kernel_intr, PM_TYPE_U32,
				&s->kernel_intr_sts);
    s->kernel_pswitch_sts =
	pmExtendFetchGroup_item(s->pmfg, "kernel.all.pswitch", NULL, NULL, &s->kernel_pswitch, PM_TYPE_U32,
				&s->kernel_pswitch_sts);
    s->kernel_cpunice_sts =
	pmExtendFetchGroup_item(s->pmfg, "kernel.all.cpu.nice", NULL, NULL, &s->kernel_cpunice, PM_TYPE_U64,
				&s->kernel_cpunice_sts);
    s->kernel_cpuuser_sts =
	pmExtendFetchGroup_item(s->pmfg, "kernel.all.cpu.user", NULL, NULL, &s->kernel_cpuuser, PM_TYPE_U64,
				&s->kernel_cpuuser_sts);
    s->kernel_cpuintr_sts =
	pmExtendFetchGroup_item(s->pmfg, "kernel.all.cpu.intr", NULL, NULL, &s->kernel_cpuintr, PM_TYPE_U64,
				&s->kernel_cpuintr_sts);
    s->kernel_cpusys_sts =
	pmExtendFetchGroup_item(s->pmfg, "kernel.all.cpu.sys", NULL, NULL, &s->kernel_cpusys, PM_TYPE_U64,
				&s->kernel_cpusys_sts);
    s->kernel_cpuidle_sts =
	pmExtendFetchGroup_item(s->pmfg, "kernel.all.cpu.idle", NULL, NULL, &s->kernel_cpuidle, PM_TYPE_U64,
				&s->kernel_cpuidle_sts);
    s->kernel_cpuwait_sts =
	pmExtendFetchGroup_item(s->pmfg, "kernel.all.cpu.wait.total", NULL, NULL, &s->kernel_cpuwait, PM_TYPE_U64,
				&s->kernel_cpuwait_sts);
    s->kernel_cpusteal_sts =
	pmExtendFetchGroup_item(s->pmfg, "kernel.all.cpu.steal", NULL, NULL, &s->kernel_cpusteal, PM_TYPE_U64,
				&s->kernel_cpusteal_sts);

    /* Let's mandate at least one metric having been resolved. */
    if (s->load_average_sts && s->swap_used_sts && s->mem_free_sts && s->mem_buf_sts && s->mem_cached_sts
	&& s->swap_in_sts && s->swap_out_sts && s->blkread_sts && s->blkwrite_sts && s->kernel_intr_sts
	&& s->kernel_pswitch_sts && s->kernel_cpunice_sts && s->kernel_cpuuser_sts && s->kernel_cpuintr_sts
	&& s->kernel_cpusys_sts && s->kernel_cpuidle_sts && s->kernel_cpuwait_sts && s->kernel_cpusteal_sts) {
	fprintf(stderr, "%s: %s: Cannot resolve any metrics.\n", pmProgname, host);
	goto out3;
    }

    goto out;

out3:
    pmDestroyFetchGroup(s->pmfg);
out2:
    pmDestroyContext(s->ctx);
out1:
    free(s);
    s = NULL;
out:
    return s;
}

static char *
saveContextHostName(int ctx)
{
    char hostname[MAXHOSTNAMELEN];
    char *name = pmGetContextHostName_r(ctx, hostname, sizeof(hostname));
    size_t length;

    if ((length = strlen(name)) == 0)
	fprintf(stderr, "%s: Warning: pmGetContextHostName(%d) failed\n", pmProgname, ctx);
    if ((name = strdup(name)) == NULL)
	__pmNoMem("context name", length + 1, PM_FATAL_ERR);
    return name;
}

static void
destroyContext(struct statsrc_t *s)
{
    if (s != NULL && s->ctx >= 0) {
	if (!s->sname)
	    s->sname = saveContextHostName(s->ctx);
	pmDestroyFetchGroup(s->pmfg);
	s->pmfg = NULL;
	pmDestroyContext(s->ctx);
	s->ctx = -1;
    }
}

static void
scalePrint(int sts, long value)
{
    if (sts)
	printf(" %4.4s", "?");
    else if (value < 10000)
	printf(" %4ld", value);
    else {
	value /= 1000;		/* '000s */
	if (value < 1000)
	    printf(" %3ldK", value);
	else {
	    value /= 1000;	/* '000,000s */
	    printf(" %3ldM", value);
	}
    }
}

static void
scaleKPrint(int sts, unsigned value)
{
    if (sts)
	printf(" %6.6s", "?");
    else if (value < 1000000)
	printf(" %6u", value);
    else {
	value /= 1024;
	if (value < 100000)
	    printf(" %5um", value);
	else {
	    value /= 1024;
	    printf(" %5ug", value);
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
setupTimeOptions(int ctx, pmOptions * opts, char **tzlabel)
{
    char *label = (char *) pmGetContextHostName(ctx);
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
    int pauseFlag = 0;
    int printTail = 0;
    int tzh = -1;
    char *tzlabel = NULL;
    char **nameList;
    int iteration;
    int nameCount;

    setlinebuf(stdout);

    while ((sts = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (sts) {
	    case 'l':		/* print last 7 characters of hostname(s) */
		printTail++;
		break;
	    case 'P':		/* pause between updates when replaying an archive */
		pauseFlag++;
		break;
	    case 'x':		/* extended CPU reporting */
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
    }
    else {
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
	}
	else {
	    __pmNoMem("contexts", nameCount * sizeof(struct statsrc_t *), PM_FATAL_ERR);
	}
    }
    else {
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
#if defined(SIGWINCH)
    __pmSetSignalHandler(SIGWINCH, resize);
#endif
    resize(0);
#endif

    /* calculate the number of samples needed, if given an end time */
    period = (opts.interval.tv_sec * 1.0e6 + opts.interval.tv_usec) / 1e6;
    now = (time_t) (opts.start.tv_sec + 0.5 + opts.start.tv_usec / 1.0e6);
    if (opts.finish_optarg) {
	double win = opts.finish.tv_sec - opts.origin.tv_sec + (opts.finish.tv_usec - opts.origin.tv_usec) / 1e6;
	win /= period;
	if (win > opts.samples)
	    opts.samples = (int) win;
    }

    if (opts.guiflag != 0 || opts.guiport != 0) {
	char *timezone = NULL;

	pmWhichZone(&timezone);
	if (!opts.guiport)
	    opts.guiport = -1;
	pmtime =
	    pmTimeStateSetup(&controls, opts.context, opts.guiport, opts.interval, opts.origin, opts.start, opts.finish,
			     timezone, tzlabel);

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

	if (pd->pmfg)
	    pmFetchGroup(pd->pmfg);
    }

    for (iteration = 0; !opts.samples || iteration < opts.samples; iteration++) {
	if ((iteration * ctxCount) % rows < ctxCount)
	    header = 1;

	if (header) {
	    char tbuf[26];

	    assert(ctxList[0] != NULL);
	    now = (time_t) (ctxList[0]->timestamp.tv_sec + 0.5 + ctxList[0]->timestamp.tv_usec / 1.0e6);
	    printf("@ %s", pmCtime(&now, tbuf));

	    if (ctxCount > 1) {
		printf("%-7s%8s%21s%10s%10s%10s%*s\n", "node", "loadavg", "memory", "swap", "io", "system",
		       extraCpuStats ? 20 : 12, "cpu");
		if (extraCpuStats)
		    printf("%8s%7s %6s %6s %6s   %c%1s   %c%1s %4s %4s %4s %4s %3s %3s %3s %3s %3s\n", "", "1 min",
			   "swpd", "buff", "cache", swapOp, "i", swapOp, "o", "bi", "bo", "in", "cs", "us", "sy", "id",
			   "wa", "st");
		else
		    printf("%8s%7s %6s %6s %6s   %c%1s   %c%1s %4s %4s %4s %4s %3s %3s %3s\n", "", "1 min", "swpd",
			   "buff", "cache", swapOp, "i", swapOp, "o", "bi", "bo", "in", "cs", "us", "sy", "id");

	    }
	    else {
		printf("%8s%28s%10s%10s%10s%*s\n", "loadavg", "memory", "swap", "io", "system", extraCpuStats ? 20 : 12,
		       "cpu");
		if (extraCpuStats)
		    printf(" %7s %6s %6s %6s %6s   %c%1s   %c%1s %4s %4s %4s %4s %3s %3s %3s %3s %3s\n", "1 min",
			   "swpd", "free", "buff", "cache", swapOp, "i", swapOp, "o", "bi", "bo", "in", "cs", "us",
			   "sy", "id", "wa", "st");
		else
		    printf(" %7s %6s %6s %6s %6s   %c%1s   %c%1s %4s %4s %4s %4s %3s %3s %3s\n", "1 min", "swpd",
			   "free", "buff", "cache", swapOp, "i", swapOp, "o", "bi", "bo", "in", "cs", "us", "sy", "id");
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
	    unsigned long long dtot = 0;
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

	    sts = pmFetchGroup(s->pmfg);
	    if (sts < 0) {
		if (opts.context == PM_CONTEXT_HOST && (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)) {
		    puts(" Fetch failed. Reconnecting ...");
		    pmReconnectContext(s->ctx);	/* XXX: safe, reusing pm* lookups? */
		}
		else if ((opts.context == PM_CONTEXT_ARCHIVE) && (sts == PM_ERR_EOL) && opts.guiflag) {
		    pmTimeStateBounds(&controls, pmtime);
		}
		else if ((opts.context == PM_CONTEXT_ARCHIVE) && (sts == PM_ERR_EOL) && (!s->fetched_p)) {
		    /* I'm yet to see something from this archive - don't
		     * discard it just yet */
		    puts(" No data in the archive");
		}
		else {
		    int k;
		    int valid = 0;

		    printf(" pmFetchGroup: %s\n", pmErrStr(sts));

		    destroyContext(s);
		    for (k = 0; k < ctxCount; k++)
			valid += (ctxList[k]->ctx >= 0);
		    if (!valid)
			exit(1);
		}
	    }
	    else {
		s->fetched_p = 1;

		if (s->load_average_sts)
		    printf(" %7.7s", "?");
		else
		    printf(" %7.2f", s->load_average.f);

		/* Memory state */
		scaleKPrint(s->swap_used_sts, s->swap_used.ul);
		scaleKPrint(s->mem_free_sts, s->mem_free.ul);
		if (ctxCount <= 1)	/* Report only for single host case */
		    scaleKPrint(s->mem_buf_sts, s->mem_buf.ul);
		scaleKPrint(s->mem_cached_sts, s->mem_cached.ul);

		/* Swap in/out */
		scalePrint(s->swap_in_sts, s->swap_in.ul);
		scalePrint(s->swap_out_sts, s->swap_out.ul);

		/* io in/out */
		scalePrint(s->blkread_sts, s->blkread.ul);
		scalePrint(s->blkwrite_sts, s->blkwrite.ul);

		/* system interrupts */
		scalePrint(s->kernel_intr_sts, s->kernel_intr.ul);
		scalePrint(s->kernel_pswitch_sts, s->kernel_pswitch.ul);

		/* CPU utilization - report percentage of total.  pmfg
		   guarantees that unavailable metrics get value 0,
		   but only after a successful pmExtendFetchGroup_*
		   call.  However, because we used calloc on the
		   statsrc_t, unavailable metrics will be 0 even in
		   the unsuccessful pmExtendFetchGroup_* case.  */
		dtot = 0;
		dtot += s->kernel_cpunice.ull;
		dtot += s->kernel_cpuuser.ull;
		dtot += s->kernel_cpuintr.ull;
		dtot += s->kernel_cpusys.ull;
		dtot += s->kernel_cpuidle.ull;
		dtot += s->kernel_cpuwait.ull;
		dtot += s->kernel_cpusteal.ull;

		if (extraCpuStats) {
		    if (dtot == 0) {
			printf(" %3.3s %3.3s %3.3s %3.3s %3.3s", "?", "?", "?", "?", "?");
		    }
		    else {
			unsigned long long fill = dtot / 2;
			printf(" %3u %3u %3u %3u %3u",
			       (unsigned int) ((100 * (s->kernel_cpunice.ull + s->kernel_cpuuser.ull) + fill) / dtot),
			       (unsigned int) ((100 * (s->kernel_cpuintr.ull + s->kernel_cpusys.ull) + fill) / dtot),
			       (unsigned int) ((100 * s->kernel_cpuidle.ull + fill) / dtot),
			       (unsigned int) ((100 * s->kernel_cpuwait.ull + fill) / dtot),
			       (unsigned int) ((100 * s->kernel_cpusteal.ull + fill) / dtot));
		    }
		}
		else if (dtot == 0) {
		    printf(" %3.3s %3.3s %3.3s", "?", "?", "?");
		}
		else {
		    /* NB: dtot will include contributions from wait/steal too. */
		    unsigned long long fill = dtot / 2;
		    printf(" %3u %3u %3u",
			   (unsigned int) ((100 * (s->kernel_cpunice.ull + s->kernel_cpuuser.ull) + fill) / dtot),
			   (unsigned int) ((100 * (s->kernel_cpuintr.ull + s->kernel_cpusys.ull) + fill) / dtot),
			   (unsigned int) ((100 * s->kernel_cpuidle.ull + fill) / dtot));
		}

		putchar('\n');
	    }
	}

next:
	if (opts.guiflag)
	    pmTimeStateAck(&controls, pmtime);

	now += (time_t) period;
    }

    exit(EXIT_SUCCESS);
}
