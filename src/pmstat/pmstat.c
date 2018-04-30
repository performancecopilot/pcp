/*
 * Copyright (c) 2013-2016 Red Hat.
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
#include "libpcp.h"
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

enum {
    load_avg,
    swap_used,
    mem_free,
    mem_buf,
    mem_cached,
    swap_in,
    swap_out,
    blk_read,
    blk_write,
    interrupts,
    pswitch,
    cpu_nice,
    cpu_user,
    cpu_intr,
    cpu_sys,
    cpu_idle,
    cpu_wait,
    cpu_steal,

    num_items
};

struct statsrc {
    pmFG		pmfg;
    char		*sname;
    struct timeval	timestamp;
    int			fetched;

    pmAtomValue		val[num_items];
    int			sts[num_items];
};

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


static struct statsrc *
getNewContext(int type, char *host, int quiet)
{
    struct statsrc *s;
    int i, sts;

    s = calloc(1, sizeof(struct statsrc));
    if (s == NULL)
	return NULL;

    sts = pmCreateFetchGroup(&s->pmfg, type, host);
    if (sts < 0) {
	if (!quiet)
	    fprintf(stderr, "%s: Cannot create fetchgroup: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	goto fail;
    }

    /* Register metrics with desired conversions/types; some with fallbacks. */
    pmExtendFetchGroup_timestamp(s->pmfg, &s->timestamp);
    s->sts[load_avg] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.load", "1 minute", NULL,
			&s->val[load_avg], PM_TYPE_FLOAT, &s->sts[load_avg]);
    s->sts[swap_used] = pmExtendFetchGroup_item(s->pmfg,
				"swap.used", NULL, "kbyte",
				&s->val[swap_used], PM_TYPE_U32,
				&s->sts[swap_used]);
    s->sts[mem_free] = pmExtendFetchGroup_item(s->pmfg,
				"mem.util.free", NULL, "kbyte",
				&s->val[mem_free], PM_TYPE_U32,
				&s->sts[mem_free]);
    s->sts[mem_buf] = pmExtendFetchGroup_item(s->pmfg,
			"mem.util.bufmem", NULL, "kbyte",
			&s->val[mem_buf], PM_TYPE_U32, &s->sts[mem_buf]);
    if (s->sts[mem_buf] < 0) {
	s->sts[mem_buf] = pmExtendFetchGroup_item(s->pmfg,
			"mem.bufmem", NULL, "kbyte",
			&s->val[mem_buf], PM_TYPE_U32, &s->sts[mem_buf]);
    }
    s->sts[mem_cached] = pmExtendFetchGroup_item(s->pmfg,
			"mem.util.cached", NULL, "kbyte",
			&s->val[mem_cached], PM_TYPE_U32, &s->sts[mem_cached]);
    s->sts[swap_in] = pmExtendFetchGroup_item(s->pmfg,
			"swap.pagesin", NULL, NULL,
			&s->val[swap_in], PM_TYPE_U32, &s->sts[swap_in]);
    if (s->sts[swap_in] < 0) {
	swapOp = 's';
	s->sts[swap_in] = pmExtendFetchGroup_item(s->pmfg,
			"swap.in", NULL, NULL,
			&s->val[swap_in], PM_TYPE_U32, &s->sts[swap_in]);
    }
    s->sts[swap_out] = pmExtendFetchGroup_item(s->pmfg,
			"swap.pagesout", NULL, NULL,
			&s->val[swap_out], PM_TYPE_U32, &s->sts[swap_out]);
    if (s->sts[swap_out] < 0) {
	swapOp = 's';
	s->sts[swap_out] = pmExtendFetchGroup_item(s->pmfg,
			"swap.out", NULL, NULL,
			&s->val[swap_out], PM_TYPE_U32, &s->sts[swap_out]);
    }
    s->sts[blk_read] = pmExtendFetchGroup_item(s->pmfg,
			"disk.all.blkread", NULL, NULL,
			&s->val[blk_read], PM_TYPE_U32, &s->sts[blk_read]);
    if (s->sts[blk_read] < 0)
	s->sts[blk_read] = pmExtendFetchGroup_item(s->pmfg,
			"disk.all.read", NULL, NULL,
			&s->val[blk_read], PM_TYPE_U32, &s->sts[blk_read]);
    s->sts[blk_write] = pmExtendFetchGroup_item(s->pmfg,
			"disk.all.blkwrite", NULL, NULL,
			&s->val[blk_write], PM_TYPE_U32, &s->sts[blk_write]);
    if (s->sts[blk_write] < 0)
	s->sts[blk_write] = pmExtendFetchGroup_item(s->pmfg,
			"disk.all.write", NULL, NULL,
			&s->val[blk_write], PM_TYPE_U32, &s->sts[blk_write]);
    s->sts[interrupts] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.intr", NULL, NULL,
			&s->val[interrupts], PM_TYPE_U32, &s->sts[interrupts]);
    s->sts[pswitch] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.pswitch", NULL, NULL,
			&s->val[pswitch], PM_TYPE_U32, &s->sts[pswitch]);
    s->sts[cpu_nice] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.cpu.nice", NULL, NULL,
			&s->val[cpu_nice], PM_TYPE_U64, &s->sts[cpu_nice]);
    s->sts[cpu_user] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.cpu.user", NULL, NULL,
			&s->val[cpu_user], PM_TYPE_U64, &s->sts[cpu_user]);
    s->sts[cpu_intr] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.cpu.intr", NULL, NULL,
			&s->val[cpu_intr], PM_TYPE_U64, &s->sts[cpu_intr]);
    s->sts[cpu_sys] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.cpu.sys", NULL, NULL,
			&s->val[cpu_sys], PM_TYPE_U64, &s->sts[cpu_sys]);
    s->sts[cpu_idle] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.cpu.idle", NULL, NULL,
			&s->val[cpu_idle], PM_TYPE_U64, &s->sts[cpu_idle]);
    s->sts[cpu_wait] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.cpu.wait.total", NULL, NULL,
			&s->val[cpu_wait], PM_TYPE_U64, &s->sts[cpu_wait]);
    s->sts[cpu_steal] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.cpu.steal", NULL, NULL,
			&s->val[cpu_steal], PM_TYPE_U64, &s->sts[cpu_steal]);

    /* Let's mandate at least one metric having been resolved. */
    for (i = 0; i < num_items; i++)
	if (s->sts[i] == 0)
	    break;
    if (i != num_items)
	return s;

    fprintf(stderr, "%s: %s: Cannot resolve any metrics.\n", pmGetProgname(), host);
    pmDestroyFetchGroup(s->pmfg);

fail:
    free(s);
    return NULL;
}

static char *
saveContextHostName(struct statsrc *s)
{
    char hostname[MAXHOSTNAMELEN];
    int ctx = pmGetFetchGroupContext(s->pmfg);
    char *name = pmGetContextHostName_r(ctx, hostname, sizeof(hostname));
    size_t length;

    if ((length = strlen(name)) == 0)
	fprintf(stderr, "%s: Warning: pmGetContextHostName(%d) failed\n",
		pmGetProgname(), ctx);
    if ((name = strdup(name)) == NULL)
	pmNoMem("context name", length + 1, PM_FATAL_ERR);
    return name;
}

static void
destroyContext(struct statsrc *s)
{
    if (s != NULL && s->pmfg != NULL) {
	if (!s->sname)
	    s->sname = saveContextHostName(s);
	pmDestroyFetchGroup(s->pmfg);
	s->pmfg = NULL;
    }
}

static void
scalePrint(struct statsrc *s, int m)
{
    unsigned long value = s->val[m].ul;

    if (s->sts[m])
	printf(" %4.4s", "?");
    else if (value < 10000)
	printf(" %4lu", value);
    else {
	value /= 1000;	/* '000s */
	if (value < 1000)
	    printf(" %3luK", value);
	else {
	    value /= 1000;	/* '000,000s */
	    printf(" %3luM", value);
	}
    }
}

static void
scaleKPrint(struct statsrc *s, int m)
{
    unsigned long value = s->val[m].ul;

    if (s->sts[m])
	printf(" %6.6s", "?");
    else if (value < 1000000)
	printf(" %6lu", value);
    else {
	value /= 1024;
	if (value < 100000)
	    printf(" %5lum", value);
	else {
	    value /= 1024;
	    printf(" %5lug", value);
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
    struct statsrc *pd;
    struct statsrc **ctxList = &pd;
    int ctxCount = 0;
    int sts, i, j;
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
	pmprintf("%s: too many options\n", pmGetProgname());
	opts.errors++;
    }

    if (pauseFlag && (opts.context != PM_CONTEXT_ARCHIVE)) {
	pmprintf("%s: -P can only be used with archives\n", pmGetProgname());
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
	if ((ctxList = calloc(nameCount, sizeof(struct statsrc *))) != NULL) {
	    int ct, fgc;

	    for (ct = 0; ct < nameCount; ct++) {
		if ((pd = getNewContext(opts.context, nameList[ct], 0)) != NULL) {
		    ctxList[ctxCount++] = pd;
		    if (tzh < 0) {
			fgc = pmGetFetchGroupContext(pd->pmfg);
			tzh = setupTimeOptions(fgc, &opts, &tzlabel);
		    }
		    pmUseZone(tzh);
		}
	    }
	} else {
	    pmNoMem("contexts", nameCount * sizeof(struct statsrc *), PM_FATAL_ERR);
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
	    tzh = setupTimeOptions(pmGetFetchGroupContext(pd->pmfg), &opts, &tzlabel);
	    ctxCount = 1;
	}
    }

    if (!ctxCount) {
	fprintf(stderr, "%s: No place to get data from!\n", pmGetProgname());
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
	char *timezone = NULL;

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
	struct statsrc *pd = ctxList[j];

	pmUseContext(pmGetFetchGroupContext(pd->pmfg));
	if (!opts.guiflag && opts.context == PM_CONTEXT_ARCHIVE)
	    pmTimeStateMode(PM_MODE_INTERP, opts.interval, &opts.origin);

	pmFetchGroup(pd->pmfg);
    }

    for (iteration = 0; !opts.samples || iteration < opts.samples; iteration++) {
	if ((iteration * ctxCount) % rows < ctxCount)
	    header = 1;

	if (header) {
	    char tbuf[26];

	    now = (time_t)(ctxList[0]->timestamp.tv_sec + 0.5 +
			   ctxList[0]->timestamp.tv_usec / 1.0e6);
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
	    struct statsrc *s = ctxList[j];

	    if (ctxCount > 1) {
		const char *fn;

		if (!s->sname)
		    s->sname = saveContextHostName(s);
		fn = s->sname;

		if (printTail)
		    printf("%-7s", strlen(fn) <= 7 ? fn : fn + strlen(fn) - 7);
		else
		    printf("%-7.7s", fn);

		if (s->pmfg == NULL) {
		    putchar('\n');
		    continue;
		}
	    }

	    sts = pmFetchGroup(s->pmfg);
	    if (sts < 0) {
		if (opts.context == PM_CONTEXT_HOST &&
		    (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)) {
		    puts(" Fetch failed. Reconnecting ...");
		    pmReconnectContext(pmGetFetchGroupContext(s->pmfg));
		} else if ((opts.context == PM_CONTEXT_ARCHIVE) &&
			 (sts == PM_ERR_EOL) && opts.guiflag) {
		    pmTimeStateBounds(&controls, pmtime);
		} else if ((opts.context == PM_CONTEXT_ARCHIVE) &&
			 (sts == PM_ERR_EOL) && (!s->fetched)) {
		    /*
		     * We are yet to see something from this archive - so
		     * don't discard it just yet.
		     */
		    puts(" No data in the archive");
		} else {
		    int valid = 0;

		    printf(" pmFetchGroup: %s\n", pmErrStr(sts));

		    destroyContext(s);
		    for (i = 0; i < ctxCount; i++)
			valid += (ctxList[i]->pmfg != NULL);
		    if (!valid)
			exit(1);
		}
	    } else {
		unsigned long long dtot = 0;
		int failed = 0;

		s->fetched = 1;

		if (s->sts[load_avg])
		    printf(" %7.7s", "?");
		else
		    printf(" %7.2f", s->val[load_avg].f);

		/* Memory state */
		scaleKPrint(s, swap_used);
		scaleKPrint(s, mem_free);
		if (ctxCount <= 1)	/* Report only for single host case */
		    scaleKPrint(s, mem_buf);
		scaleKPrint(s, mem_cached);

		/* Swap in/out */
		scalePrint(s, swap_in);
		scalePrint(s, swap_out);

		/* io in/out */
		scalePrint(s, blk_read);
		scalePrint(s, blk_write);

		/* system interrupts */
		scalePrint(s, interrupts);
		scalePrint(s, pswitch);

		/*
		 * CPU utilization - report percentage of total.
		 * Some columns are optional on some platforms, and values
		 * for these *must* be present (else no data reported) but
		 * some are optional (platform-specific).  An extended set
		 * of columns is available for Linux hosts.
		 * NB: cannot rely on any individual metric always existing
		 * here, as archives may show any combination of one, some,
		 * or all metrics logged.
		 */

		if (s->sts[cpu_user])	/* mandatory */
		    failed = 1;
		if (s->sts[cpu_sys])	/* mandatory */
		    failed = 1;
		if (s->sts[cpu_idle])	/* mandatory */
		    failed = 1;

		if (s->sts[cpu_nice])	/* optional */
		    s->val[cpu_nice].ull = 0;
		if (s->sts[cpu_intr])	/* optional */
		    s->val[cpu_intr].ull = 0;
		if (s->sts[cpu_wait])	/* optional */
		    s->val[cpu_wait].ull = 0;
		if (s->sts[cpu_steal])	/* optional */
		    s->val[cpu_steal].ull = 0;

		if (!failed)
		    for (i = cpu_nice; i <= cpu_steal; i++)
			dtot += s->val[i].ull;

		if (extraCpuStats) {
		    if (failed) { 
			printf(" %3.3s %3.3s %3.3s %3.3s %3.3s",
			       "?", "?", "?", "?", "?");
		    } else {
			unsigned long long fill = dtot / 2;
			unsigned long long user, kernel, idle, iowait, steal;

			user = s->val[cpu_nice].ull + s->val[cpu_user].ull;
			kernel = s->val[cpu_intr].ull + s->val[cpu_sys].ull;
			idle = s->val[cpu_idle].ull;
			iowait = s->val[cpu_wait].ull;
			steal = s->val[cpu_steal].ull;
			printf(" %3u %3u %3u %3u %3u",
			       (unsigned int)((100 * user + fill) / dtot),
			       (unsigned int)((100 * kernel + fill) / dtot),
			       (unsigned int)((100 * idle + fill) / dtot),
			       (unsigned int)((100 * iowait + fill) / dtot),
			       (unsigned int)((100 * steal + fill) / dtot));
		    }
		} else if (failed) {
		    printf(" %3.3s %3.3s %3.3s", "?", "?", "?");
		} else {
		    unsigned long long fill = dtot / 2;
		    unsigned long long user, kernel, idle;

		    user = s->val[cpu_nice].ull + s->val[cpu_user].ull;
		    kernel = s->val[cpu_intr].ull + s->val[cpu_sys].ull
			   + s->val[cpu_steal].ull;
		    idle = s->val[cpu_idle].ull + s->val[cpu_wait].ull;
		    printf(" %3u %3u %3u",
			   (unsigned int) ((100 * user + fill) / dtot),
			   (unsigned int) ((100 * kernel + fill) / dtot),
			   (unsigned int) ((100 * idle + fill) / dtot));
		}

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
