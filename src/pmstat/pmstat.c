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
    cpu_guest,
    cpu_guest_nice,

    num_items
};

struct statsrc {
    pmFG		pmfg;
    char		*sname;
    struct timespec	timestamp;
    int			fetched;

    pmAtomValue		val[num_items];
    int			sts[num_items];
};

/* Column group definitions */
enum column_groups {
    COL_LOADAVG,
    COL_MEMORY,
    COL_SWAP,
    COL_IO,
    COL_SYSTEM,
    COL_CPU,
    NUM_COL_GROUPS
};

/* Individual column visibility tracking */
struct display_columns {
    /* Memory group */
    int show_swap_used;
    int show_mem_free;
    int show_mem_buf;
    int show_mem_cached;

    /* Swap group */
    int show_swap_in;
    int show_swap_out;

    /* IO group */
    int show_blk_read;
    int show_blk_write;

    /* System group */
    int show_interrupts;
    int show_pswitch;

    /* CPU group - optional extended metrics */
    int show_cpu_wait;
    int show_cpu_steal;
    int show_cpu_guest;

    /* Group-level visibility (computed from individual columns) */
    int show_group[NUM_COL_GROUPS];
};

pmLongOptions longopts[] = {
    PMAPI_GENERAL_OPTIONS,
    PMOPT_DEBUG,
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
static struct display_columns display;


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
    pmExtendFetchGroup_timespec(s->pmfg, &s->timestamp);
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
    s->sts[cpu_guest] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.cpu.guest", NULL, NULL,
			&s->val[cpu_guest], PM_TYPE_U64, &s->sts[cpu_guest]);
    s->sts[cpu_guest_nice] = pmExtendFetchGroup_item(s->pmfg,
			"kernel.all.cpu.guest_nice", NULL, NULL,
			&s->val[cpu_guest_nice], PM_TYPE_U64, &s->sts[cpu_guest_nice]);

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

/*
 * Report unavailable metrics for debugging purposes.
 */
static int
reportUnavailableMetrics(void)
{
    int any_unavailable = 0;

    if (!display.show_swap_used) {
	fprintf(stderr, "  Unavailable: swap.used\n");
	any_unavailable = 1;
    }
    if (!display.show_mem_free) {
	fprintf(stderr, "  Unavailable: mem.util.free\n");
	any_unavailable = 1;
    }
    if (!display.show_mem_buf) {
	fprintf(stderr, "  Unavailable: mem.util.bufmem/mem.bufmem\n");
	any_unavailable = 1;
    }
    if (!display.show_mem_cached) {
	fprintf(stderr, "  Unavailable: mem.util.cached\n");
	any_unavailable = 1;
    }
    if (!display.show_swap_in) {
	fprintf(stderr, "  Unavailable: swap.pagesin/swap.in\n");
	any_unavailable = 1;
    }
    if (!display.show_swap_out) {
	fprintf(stderr, "  Unavailable: swap.pagesout/swap.out\n");
	any_unavailable = 1;
    }
    if (!display.show_blk_read) {
	fprintf(stderr, "  Unavailable: disk.all.blkread/disk.all.read\n");
	any_unavailable = 1;
    }
    if (!display.show_blk_write) {
	fprintf(stderr, "  Unavailable: disk.all.blkwrite/disk.all.write\n");
	any_unavailable = 1;
    }
    if (!display.show_interrupts) {
	fprintf(stderr, "  Unavailable: kernel.all.intr\n");
	any_unavailable = 1;
    }
    if (!display.show_pswitch) {
	fprintf(stderr, "  Unavailable: kernel.all.pswitch\n");
	any_unavailable = 1;
    }
    if (!display.show_cpu_wait) {
	fprintf(stderr, "  Unavailable: kernel.all.cpu.wait.total\n");
	any_unavailable = 1;
    }
    if (!display.show_cpu_steal) {
	fprintf(stderr, "  Unavailable: kernel.all.cpu.steal\n");
	any_unavailable = 1;
    }
    if (!display.show_cpu_guest) {
	fprintf(stderr, "  Unavailable: kernel.all.cpu.guest/guest_nice\n");
	any_unavailable = 1;
    }

    return any_unavailable;
}

/*
 * Report suppressed columns for debugging purposes.
 */
static void
reportSuppressedColumns(void)
{
    fprintf(stderr, "\nSuppressed columns:\n");

    if (!display.show_pswitch)
	fprintf(stderr, "  - pswitch (cs)\n");
    if (!display.show_mem_buf)
	fprintf(stderr, "  - mem.bufmem (buff)\n");
    if (!display.show_swap_in)
	fprintf(stderr, "  - swap.in (si)\n");
    if (!display.show_swap_out)
	fprintf(stderr, "  - swap.out (so)\n");
    if (!display.show_mem_cached)
	fprintf(stderr, "  - mem.cached (cache)\n");
    if (!display.show_cpu_wait && extraCpuStats)
	fprintf(stderr, "  - cpu.wait (wa)\n");
    if (!display.show_cpu_steal && extraCpuStats)
	fprintf(stderr, "  - cpu.steal (st)\n");
    if (!display.show_cpu_guest && extraCpuStats)
	fprintf(stderr, "  - cpu.guest (gu)\n");
}

/*
 * Report group visibility for debugging purposes.
 */
static void
reportGroupVisibility(void)
{
    fprintf(stderr, "\nGroup visibility:\n");

    if (!display.show_group[COL_MEMORY])
	fprintf(stderr, "  - memory group: HIDDEN (no metrics available)\n");
    else
	fprintf(stderr, "  - memory group: visible\n");

    if (!display.show_group[COL_SWAP])
	fprintf(stderr, "  - swap group: HIDDEN (no metrics available)\n");
    else
	fprintf(stderr, "  - swap group: visible\n");

    if (!display.show_group[COL_IO])
	fprintf(stderr, "  - io group: HIDDEN (no metrics available)\n");
    else
	fprintf(stderr, "  - io group: visible\n");

    if (!display.show_group[COL_SYSTEM])
	fprintf(stderr, "  - system group: HIDDEN (no metrics available)\n");
    else
	fprintf(stderr, "  - system group: visible\n");
}

/*
 * Report diagnostic information about column availability.
 */
static void
reportAvailabilityDiagnostics(void)
{
    int any_unavailable;

    fprintf(stderr, "Column availability analysis:\n");

    any_unavailable = reportUnavailableMetrics();
    reportSuppressedColumns();
    reportGroupVisibility();

    if (!any_unavailable)
	fprintf(stderr, "\n  All metrics available\n");

    fprintf(stderr, "\n");
}

/*
 * Analyze metric availability across all contexts.
 * If ANY context lacks a metric, mark it as unavailable globally.
 * This ensures consistent column layout across all hosts.
 */
static void
detectColumnAvailability(struct statsrc **ctxList, int ctxCount)
{
    int i;

    /* Start optimistic - assume all columns available */
    memset(&display, 1, sizeof(display));

    /* Check each context */
    for (i = 0; i < ctxCount; i++) {
	struct statsrc *s = ctxList[i];

	/* Memory group */
	if (s->sts[swap_used]) display.show_swap_used = 0;
	if (s->sts[mem_free]) display.show_mem_free = 0;
	if (s->sts[mem_buf]) display.show_mem_buf = 0;
	if (s->sts[mem_cached]) display.show_mem_cached = 0;

	/* Swap group */
	if (s->sts[swap_in]) display.show_swap_in = 0;
	if (s->sts[swap_out]) display.show_swap_out = 0;

	/* IO group */
	if (s->sts[blk_read]) display.show_blk_read = 0;
	if (s->sts[blk_write]) display.show_blk_write = 0;

	/* System group */
	if (s->sts[interrupts]) display.show_interrupts = 0;
	if (s->sts[pswitch]) display.show_pswitch = 0;

	/* CPU optional metrics */
	if (s->sts[cpu_wait]) display.show_cpu_wait = 0;
	if (s->sts[cpu_steal]) display.show_cpu_steal = 0;
	if (s->sts[cpu_guest] && s->sts[cpu_guest_nice])
	    display.show_cpu_guest = 0;
    }

    /* Determine group-level visibility */
    display.show_group[COL_LOADAVG] = 1;  /* Always show if we have any data */

    display.show_group[COL_MEMORY] =
	display.show_swap_used || display.show_mem_free ||
	display.show_mem_buf || display.show_mem_cached;

    display.show_group[COL_SWAP] =
	display.show_swap_in || display.show_swap_out;

    display.show_group[COL_IO] =
	display.show_blk_read || display.show_blk_write;

    display.show_group[COL_SYSTEM] =
	display.show_interrupts || display.show_pswitch;

    display.show_group[COL_CPU] = 1;  /* CPU always shown */

    /* Verbose reporting - only if debugging enabled */
    if (pmDebugOptions.appl0)
	reportAvailabilityDiagnostics();
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
    opts.interval.tv_sec = delta.tv_sec;
    opts.interval.tv_nsec = delta.tv_usec * 1000;
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

/*
 * Calculate display width for the memory group based on visible columns.
 */
static int
calculateMemoryWidth(int multiHost)
{
    int width = 0;
    if (display.show_swap_used) width += 7;  /* " %6s" */
    if (display.show_mem_free) width += 7;
    if (display.show_mem_buf && !multiHost) width += 7;
    if (display.show_mem_cached) width += 7;
    return width;
}

/*
 * Calculate display width for the swap group based on visible columns.
 */
static int
calculateSwapWidth(void)
{
    int width = 0;
    if (display.show_swap_in) width += 5;   /* "   %c%1s" */
    if (display.show_swap_out) width += 5;
    return width;
}

/*
 * Calculate display width for the IO group based on visible columns.
 */
static int
calculateIoWidth(void)
{
    int width = 0;
    if (display.show_blk_read) width += 5;  /* " %4s" */
    if (display.show_blk_write) width += 5;
    return width;
}

/*
 * Calculate display width for the system group based on visible columns.
 */
static int
calculateSystemWidth(void)
{
    int width = 0;
    if (display.show_interrupts) width += 5;  /* " %4s" */
    if (display.show_pswitch) width += 5;
    return width;
}

/*
 * Print column headers dynamically based on availability.
 */
static void
printColumnHeaders(int multiHost)
{
    if (multiHost)
	printf("%8s", "");  /* Align with node name */

    /* Load average */
    printf(" %7s", "1 min");

    /* Memory group - conditional */
    if (display.show_swap_used)
	printf(" %6s", "swpd");
    if (display.show_mem_free) {
	if (multiHost)
	    printf(" %6s", "");  /* skip "free" label in multi-host */
	else
	    printf(" %6s", "free");
    }
    if (display.show_mem_buf && !multiHost)
	printf(" %6s", "buff");
    if (display.show_mem_cached)
	printf(" %6s", "cache");

    /* Swap group - conditional */
    if (display.show_swap_in)
	printf("   %c%1s", swapOp, "i");
    if (display.show_swap_out)
	printf("   %c%1s", swapOp, "o");

    /* IO group - conditional */
    if (display.show_blk_read)
	printf(" %4s", "bi");
    if (display.show_blk_write)
	printf(" %4s", "bo");

    /* System group - conditional */
    if (display.show_interrupts)
	printf(" %4s", "in");
    if (display.show_pswitch)
	printf(" %4s", "cs");

    /* CPU group - always shown */
    printf(" %3s %3s %3s", "us", "sy", "id");
    if (extraCpuStats) {
	if (display.show_cpu_wait)
	    printf(" %3s", "wa");
	if (display.show_cpu_steal)
	    printf(" %3s", "st");
	if (display.show_cpu_guest)
	    printf(" %3s", "gu");
    }

    printf("\n");
}

static void
printGroupHeaders(int multiHost)
{
    int mem_width, swap_width, io_width, sys_width;

    if (multiHost)
	printf("%-7s", "node");

    printf("%8s", "loadavg");

    /* Memory group - dynamic width based on available columns */
    mem_width = calculateMemoryWidth(multiHost);
    if (mem_width > 0)
	printf("%*s", mem_width, "memory");

    /* Swap group - dynamic width based on available columns */
    swap_width = calculateSwapWidth();
    if (swap_width > 0)
	printf("%*s", swap_width, "swap");

    /* IO group - dynamic width based on available columns */
    io_width = calculateIoWidth();
    if (io_width > 0)
	printf("%*s", io_width, "io");

    /* System group - dynamic width based on available columns */
    sys_width = calculateSystemWidth();
    if (sys_width > 0)
	printf("%*s", sys_width, "system");

    /* CPU group - dynamic width based on extended stats */
    if (extraCpuStats) {
	int cpu_width = 12;  /* Base: us sy id */
	if (display.show_cpu_wait) cpu_width += 4;
	if (display.show_cpu_steal) cpu_width += 4;
	if (display.show_cpu_guest) cpu_width += 4;
	printf("%*s\n", cpu_width, "cpu");
    } else {
	printf("%*s\n", 12, "cpu");
    }
}

static int
setupTimeOptions(int ctx, pmOptions *optsp, char **tzlabel)
{
    char *label = (char *)pmGetContextHostName(ctx);
    char *zone;

    if (pmGetContextOptions(ctx, optsp)) {
	pmflush();
	exit(1);
    }
    if (optsp->timezone)
	*tzlabel = optsp->timezone;
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
    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmUsageMessage(&opts);
	exit(0);
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

    if (opts.interval.tv_sec == 0 && opts.interval.tv_nsec == 0)
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

    /* Detect which columns can be displayed based on metric availability */
    detectColumnAvailability(ctxList, ctxCount);

#if defined(TIOCGWINSZ)
# if defined(SIGWINCH)
    __pmSetSignalHandler(SIGWINCH, resize);
# endif
    resize(0);
#endif

    /* calculate the number of samples needed, if given an end time */
    period = (opts.interval.tv_sec * 1.0e6 + opts.interval.tv_nsec) / 1e9;
    now = (time_t)(opts.start.tv_sec + 0.5 + opts.start.tv_nsec / 1.0e9);
    if (opts.finish_optarg) {
	double win = opts.finish.tv_sec - opts.origin.tv_sec +
		    (opts.finish.tv_nsec - opts.origin.tv_nsec) / 1e9;
	win /= period;
	if (win > opts.samples)
	    opts.samples = (int)win;
    }

    if (opts.guiflag != 0 || opts.guiport != 0) {
	char *tz = NULL;

	pmWhichZone(&tz);
	if (!opts.guiport)
	    opts.guiport = -1;
#ifdef PMTIME_FIXED
	pmtime = pmTimeStateSetup(&controls, opts.context,
			opts.guiport, opts.interval, opts.origin,
			opts.start, opts.finish, tz, tzlabel);
#else
	{
	    struct timeval	interval_tv = { opts.interval.tv_sec, opts.interval.tv_nsec / 1000 };
	    struct timeval	origin_tv = { opts.origin.tv_sec, opts.origin.tv_nsec / 1000 };
	    struct timeval	start_tv = { opts.start.tv_sec, opts.start.tv_nsec / 1000 };
	    struct timeval	finish_tv = { opts.finish.tv_sec, opts.finish.tv_nsec / 1000 };
	    pmtime = pmTimeStateSetup(&controls, opts.context,
			opts.guiport, interval_tv, origin_tv,
			start_tv, finish_tv, tz, tzlabel);
	}
#endif

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
	pd = ctxList[j];

	pmUseContext(pmGetFetchGroupContext(pd->pmfg));
	if (!opts.guiflag && opts.context == PM_CONTEXT_ARCHIVE) {
	    struct timeval	interval_tv = { opts.interval.tv_sec, opts.interval.tv_nsec / 1000 };
	    struct timeval	origin_tv = { opts.origin.tv_sec, opts.origin.tv_nsec / 1000 };
	    pmTimeStateMode(PM_MODE_INTERP, interval_tv, &origin_tv);
	}

	pmFetchGroup(pd->pmfg);
    }

    for (iteration = 0; !opts.samples || iteration < opts.samples; iteration++) {
	if ((iteration * ctxCount) % rows < ctxCount)
	    header = 1;

	if (header) {
	    char tbuf[26];

	    now = (time_t)(ctxList[0]->timestamp.tv_sec + 0.5 +
			   ctxList[0]->timestamp.tv_nsec / 1.0e9);
	    printf("@ %s", pmCtime(&now, tbuf));

	    printGroupHeaders(ctxCount > 1);
	    printColumnHeaders(ctxCount > 1);
	    header = 0;
	}

	if (opts.guiflag)
	    pmTimeStateVector(&controls, pmtime);
	else if (opts.context != PM_CONTEXT_ARCHIVE || pauseFlag)
	    __pmtimespecSleep(opts.interval);
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
		int	valid;
		if (opts.context == PM_CONTEXT_HOST &&
		    (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)) {
		    puts(" Fetch failed. Reconnecting ...");
		    sts = pmReconnectContext(pmGetFetchGroupContext(s->pmfg));
		    if (sts < 0) {
			printf(" pmReconnectContext: %s\n", pmErrStr(sts));
			goto check;
		    }
		} else if ((opts.context == PM_CONTEXT_ARCHIVE) &&
			 (sts == PM_ERR_EOL) && opts.guiflag) {
		    pmTimeStateBounds(&controls, pmtime);
		} else if ((opts.context == PM_CONTEXT_ARCHIVE) &&
			 (sts == PM_ERR_EOL) && (!s->fetched)) {
		    /*
		     * We are yet to see something from this archive and
		     * we're at the end of the archive.
		     */
		    puts(" No data in the archive");
		    printf(" pmFetchGroup: %s\n", pmErrStr(sts));
		    exit(0);
		} else {
		    printf(" pmFetchGroup: %s\n", pmErrStr(sts));

check:
		    valid = 0;
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
		if (display.show_swap_used)
		    scaleKPrint(s, swap_used);
		if (display.show_mem_free)
		    scaleKPrint(s, mem_free);
		if (display.show_mem_buf && ctxCount <= 1)
		    scaleKPrint(s, mem_buf);
		if (display.show_mem_cached)
		    scaleKPrint(s, mem_cached);

		/* Swap in/out */
		if (display.show_swap_in)
		    scalePrint(s, swap_in);
		if (display.show_swap_out)
		    scalePrint(s, swap_out);

		/* IO in/out */
		if (display.show_blk_read)
		    scalePrint(s, blk_read);
		if (display.show_blk_write)
		    scalePrint(s, blk_write);

		/* system interrupts */
		if (display.show_interrupts)
		    scalePrint(s, interrupts);
		if (display.show_pswitch)
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
		if (s->sts[cpu_guest])	/* optional */
		    s->val[cpu_guest].ull = 0;
		if (s->sts[cpu_guest_nice])	/* optional */
		    s->val[cpu_guest_nice].ull = 0;

		if (!failed)
		    for (i = cpu_nice; i <= cpu_guest_nice; i++)
			dtot += s->val[i].ull;

		if (extraCpuStats) {
		    if (failed) {
			printf(" %3.3s %3.3s %3.3s %3.3s %3.3s %3.3s",
			       "?", "?", "?", "?", "?", "?");
		    } else {
			unsigned long long fill = dtot / 2;
			unsigned long long user, kernel, idle, iowait, steal, guest;

			user = s->val[cpu_nice].ull + s->val[cpu_user].ull;
			kernel = s->val[cpu_intr].ull + s->val[cpu_sys].ull;
			idle = s->val[cpu_idle].ull;
			iowait = s->val[cpu_wait].ull;
			steal = s->val[cpu_steal].ull;
			guest = s->val[cpu_guest_nice].ull + s->val[cpu_guest].ull;
			printf(" %3u %3u %3u %3u %3u %3u",
			       (unsigned int)((100 * user + fill) / dtot),
			       (unsigned int)((100 * kernel + fill) / dtot),
			       (unsigned int)((100 * idle + fill) / dtot),
			       (unsigned int)((100 * iowait + fill) / dtot),
			       (unsigned int)((100 * steal + fill) / dtot),
			       (unsigned int)((100 * guest + fill) / dtot));
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
