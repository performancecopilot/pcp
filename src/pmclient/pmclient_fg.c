/*
 * pmclient_fg - sample, simpler PMAPI/fetchgroup client
 *
 * Copyright (c) 2013-2015 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include <errno.h>
#include <assert.h>

pmLongOptions longopts[] = {
    PMAPI_GENERAL_OPTIONS,
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "pause", 0, 'P', 0, "pause between updates for archive replay" },
    PMAPI_OPTIONS_END
};

pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ | PM_OPTFLAG_BOUNDARIES,
    .short_options = PMAPI_OPTIONS "P",
    .long_options = longopts,
};

typedef struct {
    struct timeval	timestamp;	/* last fetched time */
    double		cpu_util;	/* aggregate CPU utilization, usr+sys */
    int			peak_cpu;	/* most utilized CPU, if > 1 CPU */
    double		peak_cpu_util;	/* utilization for most utilized CPU */
    pmAtomValue		freemem;	/* free memory (Mbytes) */
    pmAtomValue		dkiops;		/* aggregate disk I/O's per second */
    pmAtomValue		load1;		/* 1 minute load average */
    pmAtomValue		load15;		/* 15 minute load average */
    pmAtomValue		ncpu;		/* number of cpus */
    unsigned int	last_ncpu;	/* last seen number of cpus */
} info_t;

static info_t		info;
static pmFG		pmfg;

static void
get_sample(void)
{
    enum { indom_maxnum = 1024 };
    static int		cpu_user_inst[indom_maxnum]; 
    static pmAtomValue 	cpu_user[indom_maxnum];
    static unsigned	num_cpu_user;
    static int		cpu_sys_inst[indom_maxnum];
    static pmAtomValue	cpu_sys[indom_maxnum];
    static unsigned	num_cpu_sys;
    static int		setup;
    int			sts;
    int			i;

    if (!setup) {
	setup = 1;

	if ((sts = pmExtendFetchGroup_item(pmfg, "hinv.ncpu", NULL, NULL,
					   &info.ncpu, PM_TYPE_32, &sts)) < 0) {
	    fprintf(stderr, "%s: Failed hinv.ncpu ExtendFetchGroup: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
	
	/*
	 * Because of pmfg_item's willingness to scan to the end of an
	 * archive to do metric/instance resolution, we don't have to
	 * specially handle the PM_CONTEXT_ARCHIVE case here.
	 */

	if ((sts = pmExtendFetchGroup_item(pmfg,
				"kernel.all.load", "1 minute", NULL,
				&info.load1, PM_TYPE_DOUBLE, NULL)) < 0) {
	    fprintf(stderr, "%s: Failed kernel.all.load[1] "
			    "ExtendFetchGroup: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
	if ((sts = pmExtendFetchGroup_item(pmfg,
				"kernel.all.load", "15 minute", NULL,
				&info.load15, PM_TYPE_DOUBLE, NULL)) < 0) {
	    fprintf(stderr, "%s: Failed kernel.all.load[15] "
			    "ExtendFetchGroup: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
	if ((sts = pmExtendFetchGroup_indom(pmfg,
				"kernel.percpu.cpu.user", "second/second",
				cpu_user_inst, NULL, cpu_user, PM_TYPE_DOUBLE,
				NULL, indom_maxnum, &num_cpu_user, NULL)) < 0) {
	    fprintf(stderr, "%s: Failed kernel.percpu.cpu.user "
				"ExtendFetchGroup: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}

	if ((sts = pmExtendFetchGroup_indom(pmfg,
				"kernel.percpu.cpu.sys", "second/second",
				cpu_sys_inst, NULL, cpu_sys, PM_TYPE_DOUBLE,
				NULL, indom_maxnum, &num_cpu_sys, NULL)) < 0) {
	    fprintf(stderr, "%s: Failed kernel.percpu.cpu.sys "
				"ExtendFetchGroup: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
	if ((sts = pmExtendFetchGroup_item(pmfg,
				"mem.freemem", NULL, "Mbyte",
				&info.freemem, PM_TYPE_DOUBLE, NULL)) < 0) {
	    fprintf(stderr, "%s: Failed mem.freemem "
				"ExtendFetchGroup: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
	if ((sts = pmExtendFetchGroup_item(pmfg,
				"disk.all.total", NULL, "count/second",
				&info.dkiops, PM_TYPE_32, NULL)) < 0) {
	    fprintf(stderr, "%s: Failed disk.all.total "
				"ExtendFetchGroup: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
	if ((sts = pmExtendFetchGroup_timestamp(pmfg, &info.timestamp)) < 0) {
	    fprintf(stderr, "%s: Failed ExtendFetchGroup: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}

	/*
	 * Since we don't have a "last" call, we will have some
	 * some memory at exit, namely the cpu_sys and cpu_user
	 * arrays, and the object hiding behind pmfg.
	 */
    }

    /*
     * Fetch the current metrics; fill many info.* fields.  Since we
     * passed NULLs to most fetchgroup status int*'s, we'll get
     * PM_TYPE_DOUBLE fetch/conversion errors represented by NaN's.
     */
    sts = pmFetchGroup(pmfg);
    if (sts < 0) {
	fprintf(stderr, "%s: pmFetchGroup: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /* compute rate-converted values */
    info.cpu_util = 0;
    info.peak_cpu_util = -1;	/* force re-assignment at first CPU */

    /*
     * Safely assume that the cpu_user and cpu_sys indoms are identical
     * and that each has a corresponding set of values, so we zip them
     * up pairwise with one iteration and no auxiliary data structures.
     */
    assert(num_cpu_user == num_cpu_sys);
    for (i = 0; i < num_cpu_user; i++) {
	double util;

	/* corresponding instances */
	assert(cpu_user_inst[i] == cpu_sys_inst[i]);

	util = cpu_user[i].d + cpu_sys[i].d; /* already rate-converted */
	if (util > 1.0)
	    /* small errors are possible, so clip the utilization at 1.0 */
	    util = 1.0;
	info.cpu_util += util;
	if (util > info.peak_cpu_util) {
	    info.peak_cpu_util = util;
	    /* NB: i is indom instance, not result index */
	    info.peak_cpu = cpu_user_inst[i];
	}
    }
    assert(info.ncpu.l != 0);
    info.cpu_util /= info.ncpu.l;
}

void
timeval_sleep(struct timeval delay)
{
    struct timespec	interval;
    struct timespec	remaining;

    interval.tv_sec = delay.tv_sec;
    interval.tv_nsec = delay.tv_usec * 1000;

    /* loop to catch early wakeup by nanosleep */
    for (;;) {
	int sts = nanosleep(&interval, &remaining);
	if (sts == 0 || (sts < 0 && errno != EINTR))
	    break;
	interval = remaining;
    }
}

int
main(int argc, char **argv)
{
    int			c;
    int			sts;
    int			samples;
    int			pauseFlag = 0;
    int			lines = 0;
    char		*source;
    const char		*host;
    char		timebuf[26];	/* for pmCtime result */

    setlinebuf(stdout);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'P':
	    pauseFlag++;
	    break;
	default:
	    opts.errors++;
	    break;
	}
    }

    if (pauseFlag && opts.context != PM_CONTEXT_ARCHIVE) {
	pmprintf("%s: pause can only be used with archives\n", pmGetProgname());
	opts.errors++;
    }
    if (opts.optind < argc - 1)
	opts.errors++;
    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	sts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(sts);
    }

    if (opts.context == PM_CONTEXT_ARCHIVE) {
	source = opts.archives[0];
    } else if (opts.context == PM_CONTEXT_HOST) {
	source = opts.hosts[0];
    } else {
	opts.context = PM_CONTEXT_HOST;
	source = "local:";
    }

    sts = pmCreateFetchGroup(& pmfg, opts.context, source);
    if (sts < 0) {
	if (opts.context == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmGetProgname(), source, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), source, pmErrStr(sts));
	exit(1);
    }
    c = pmGetFetchGroupContext(pmfg);
    
    /* complete TZ and time window option (origin) setup */
    if (pmGetContextOptions(c, &opts)) {
	pmflush();
	exit(1);
    }

    host = pmGetContextHostName(c);

    /* set a default sampling interval if none has been requested */
    if (opts.interval.tv_sec == 0 && opts.interval.tv_usec == 0)
	opts.interval.tv_sec = 5;

    if (opts.context == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmSetMode(PM_MODE_INTERP, &opts.start, (int)(opts.interval.tv_sec*1000 + opts.interval.tv_usec/1000))) < 0) {
	    fprintf(stderr, "%s: pmSetMode failed: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
    }

    if (opts.context == PM_CONTEXT_ARCHIVE)
	get_sample(); /* fetch the separate early ncpu record */
    get_sample(); /* fetch other rate metrics */

    /* set sampling loop termination via the command line options */
    samples = opts.samples ? opts.samples : -1;

    while (samples == -1 || samples-- > 0) {
	if (lines % 15 == 0) {
	    time_t	now = info.timestamp.tv_sec;
	    if (opts.context == PM_CONTEXT_ARCHIVE)
		printf("Archive: %s, ", opts.archives[0]);
	    printf("Host: %s, %d cpu(s), %s",
		   host, info.ncpu.l,
		   pmCtime(&now, timebuf));
/* - report format
  CPU  Busy    Busy  Free Mem   Disk     Load Average
 Util   CPU    Util  (Mbytes)   IOPS    1 Min  15 Min
X.XXX   XXX   X.XXX XXXXX.XXX XXXXXX  XXXX.XX XXXX.XX
*/
	    printf("  CPU");
	    if (info.ncpu.l > 1)
		printf("  Busy    Busy");
	    printf("  Free Mem   Disk     Load Average\n");
	    printf(" Util");
	    if (info.ncpu.l > 1)
		printf("   CPU    Util");
	    printf("  (Mbytes)   IOPS    1 Min  15 Min\n");
	}
	if (opts.context != PM_CONTEXT_ARCHIVE || pauseFlag)
	    timeval_sleep(opts.interval);
	get_sample();
	printf("%5.2f", info.cpu_util);
	if (info.ncpu.l > 1)
	    printf("   %3d   %5.2f", info.peak_cpu, info.peak_cpu_util);
	printf(" %9.3f", info.freemem.d);
	printf(" %6d", info.dkiops.l);
	printf("  %7.2f %7.2f\n", info.load1.d, info.load15.d);
 	lines++;
    }
    exit(0);
}
