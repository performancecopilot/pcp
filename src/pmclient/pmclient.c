/*
 * pmclient - sample, simple PMAPI client
 *
 * Copyright (c) 2013-2014 Red Hat.
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
#include "pmnsmap.h"
#include <errno.h>

pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_ALIGN,
    PMOPT_ARCHIVE,
    PMOPT_DEBUG,
    PMOPT_HOST,
    PMOPT_NAMESPACE,
    PMOPT_ORIGIN,
    PMOPT_START,
    PMOPT_SAMPLES,
    PMOPT_FINISH,
    PMOPT_INTERVAL,
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_VERSION,
    PMOPT_HELP,
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
    float		cpu_util;	/* aggregate CPU utilization, usr+sys */
    int			peak_cpu;	/* most utilized CPU, if > 1 CPU */
    float		peak_cpu_util;	/* utilization for most utilized CPU */
    float		freemem;	/* free memory (Mbytes) */
    int			dkiops;		/* aggregate disk I/O's per second */
    float		load1;		/* 1 minute load average */
    float		load15;		/* 15 minute load average */
} info_t;

static unsigned int	ncpu;

static unsigned int
get_ncpu(void)
{
    /* there is only one metric in the pmclient_init group */
    pmID	pmidlist[1];
    pmDesc	desclist[1];
    pmResult	*rp;
    pmAtomValue	atom;
    int		sts;

    if ((sts = pmLookupName(1, (const char **)pmclient_init, pmidlist)) < 0) {
	fprintf(stderr, "%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
	fprintf(stderr, "%s: metric \"%s\" not in name space\n",
			pmGetProgname(), pmclient_init[0]);
	exit(1);
    }
    if ((sts = pmLookupDesc(pmidlist[0], desclist)) < 0) {
	fprintf(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
		pmGetProgname(), pmclient_init[0], pmIDStr(pmidlist[0]), pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetch(1, pmidlist, &rp)) < 0) {
	fprintf(stderr, "%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /* the thing we want is known to be the first value */
    pmExtractValue(rp->vset[0]->valfmt, rp->vset[0]->vlist, desclist[0].type,
		   &atom, PM_TYPE_U32);
    pmFreeResult(rp);

    return atom.ul;
}

static void
get_sample(info_t *ip)
{
    static pmResult	*crp = NULL;	/* current */
    static pmResult	*prp = NULL;	/* prior */
    static int		first = 1;
    static int		numpmid;
    static pmID		*pmidlist;
    static pmDesc	*desclist;
    static int		inst1;
    static int		inst15;
    static pmUnits	mbyte_scale;

    int			sts;
    int			i;
    float		u;
    pmAtomValue		tmp;
    pmAtomValue		atom;
    double		dt;

    if (first) {
	/* first time initialization */
	mbyte_scale.dimSpace = 1;
	mbyte_scale.scaleSpace = PM_SPACE_MBYTE;

	numpmid = sizeof(pmclient_sample) / sizeof(char *);
	if ((pmidlist = (pmID *)malloc(numpmid * sizeof(pmidlist[0]))) == NULL) {
	    fprintf(stderr, "%s: get_sample: malloc: %s\n", pmGetProgname(), osstrerror());
	    exit(1);
	}
	if ((desclist = (pmDesc *)malloc(numpmid * sizeof(desclist[0]))) == NULL) {
	    fprintf(stderr, "%s: get_sample: malloc: %s\n", pmGetProgname(), osstrerror());
	    exit(1);
	}
	if ((sts = pmLookupName(numpmid, (const char **)pmclient_sample, pmidlist)) < 0) {
	    printf("%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
	    for (i = 0; i < numpmid; i++) {
		if (pmidlist[i] == PM_ID_NULL)
		    fprintf(stderr, "%s: metric \"%s\" not in name space\n", pmGetProgname(), pmclient_sample[i]);
	    }
	    exit(1);
	}
	for (i = 0; i < numpmid; i++) {
	    if ((sts = pmLookupDesc(pmidlist[i], &desclist[i])) < 0) {
		fprintf(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
		    pmGetProgname(), pmclient_sample[i], pmIDStr(pmidlist[i]), pmErrStr(sts));
		exit(1);
	    }
	}
    }

    /* fetch the current metrics */
    if ((sts = pmFetch(numpmid, pmidlist, &crp)) < 0) {
	fprintf(stderr, "%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    if (first) {
	/*
	 * from now on, just want the 1 minute and 15 minute load averages,
	 * so limit the instance profile for this metric
	 */
	if (opts.context == PM_CONTEXT_ARCHIVE) {
	    inst1 = pmLookupInDomArchive(desclist[LOADAV].indom, "1 minute");
	    inst15 = pmLookupInDomArchive(desclist[LOADAV].indom, "15 minute");
	}
	else {
	    inst1 = pmLookupInDom(desclist[LOADAV].indom, "1 minute");
	    inst15 = pmLookupInDom(desclist[LOADAV].indom, "15 minute");
	}
	if (inst1 < 0) {
	    fprintf(stderr, "%s: cannot translate instance for 1 minute load average\n", pmGetProgname());
	    exit(1);
	}
	if (inst15 < 0) {
	    fprintf(stderr, "%s: cannot translate instance for 15 minute load average\n", pmGetProgname());
	    exit(1);
	}
	pmDelProfile(desclist[LOADAV].indom, 0, NULL);	/* all off */
	pmAddProfile(desclist[LOADAV].indom, 1, &inst1);
	pmAddProfile(desclist[LOADAV].indom, 1, &inst15);

	first = 0;
    }

    /* if the second or later sample, pick the results apart */
    if (prp !=  NULL) {

	dt = pmtimevalSub(&crp->timestamp, &prp->timestamp);

	/*
	 * But first ... is all the data present?
	 */
	if (prp->vset[CPU_USR]->numval <= 0 || crp->vset[CPU_USR]->numval <= 0 ||
	    prp->vset[CPU_SYS]->numval <= 0 || crp->vset[CPU_SYS]->numval <= 0) {
	    ip->cpu_util = -1;
	    ip->peak_cpu = -1;
	    ip->peak_cpu_util = -1;
	}
	else {
	    ip->cpu_util = 0;
	    ip->peak_cpu_util = -1;	/* force re-assignment at first CPU */
	    for (i = 0; i < ncpu; i++) {
		pmExtractValue(crp->vset[CPU_USR]->valfmt,
			       &crp->vset[CPU_USR]->vlist[i],
			       desclist[CPU_USR].type, &atom, PM_TYPE_FLOAT);
		u = atom.f;
		pmExtractValue(prp->vset[CPU_USR]->valfmt,
			       &prp->vset[CPU_USR]->vlist[i],
			       desclist[CPU_USR].type, &atom, PM_TYPE_FLOAT);
		u -= atom.f;
		pmExtractValue(crp->vset[CPU_SYS]->valfmt,
			       &crp->vset[CPU_SYS]->vlist[i],
			       desclist[CPU_SYS].type, &atom, PM_TYPE_FLOAT);
		u += atom.f;
		pmExtractValue(prp->vset[CPU_SYS]->valfmt,
			       &prp->vset[CPU_SYS]->vlist[i],
			       desclist[CPU_SYS].type, &atom, PM_TYPE_FLOAT);
		u -= atom.f;
		/*
		 * really should use pmConvertValue, but I _know_ the times
		 * are in msec!
		 */
		u = u / (1000 * dt);

		if (u > 1.0)
		    /* small errors are possible, so clip the utilization at 1.0 */
		    u = 1.0;
		ip->cpu_util += u;
		if (u > ip->peak_cpu_util) {
		    ip->peak_cpu_util = u;
		    ip->peak_cpu = i;
		}
	    }
	    ip->cpu_util /= ncpu;
	}

	/* freemem - expect just one value */
	if (prp->vset[FREEMEM]->numval <= 0 || crp->vset[FREEMEM]->numval <= 0) {
	    ip->freemem = -1;
	}
	else {
	    pmExtractValue(crp->vset[FREEMEM]->valfmt, crp->vset[FREEMEM]->vlist,
		    desclist[FREEMEM].type, &tmp, PM_TYPE_FLOAT);
	    /* convert from today's units at the collection site to Mbytes */
	    sts = pmConvScale(PM_TYPE_FLOAT, &tmp, &desclist[FREEMEM].units,
		    &atom, &mbyte_scale);
	    if (sts < 0) {
		/* should never happen */
		if (pmDebugOptions.value) {
		    fprintf(stderr, "%s: get_sample: Botch: %s (%s) scale conversion from %s", 
			pmGetProgname(), pmIDStr(desclist[FREEMEM].pmid), pmclient_sample[FREEMEM], pmUnitsStr(&desclist[FREEMEM].units));
		    fprintf(stderr, " to %s failed: %s\n", pmUnitsStr(&mbyte_scale), pmErrStr(sts));
		}
		ip->freemem = 0;
	    }
	    else
		ip->freemem = atom.f;
	}

	/* disk IOPS - expect just one value, but need delta */
	if (prp->vset[DKIOPS]->numval <= 0 || crp->vset[DKIOPS]->numval <= 0) {
	    ip->dkiops = -1;
	}
	else {
	    pmExtractValue(crp->vset[DKIOPS]->valfmt, crp->vset[DKIOPS]->vlist,
			desclist[DKIOPS].type, &atom, PM_TYPE_U32);
	    ip->dkiops = atom.ul;
	    pmExtractValue(prp->vset[DKIOPS]->valfmt, prp->vset[DKIOPS]->vlist,
			desclist[DKIOPS].type, &atom, PM_TYPE_U32);
	    ip->dkiops -= atom.ul;
	    ip->dkiops = ((float)(ip->dkiops) + 0.5) / dt;
	}

	/* load average ... process all values, matching up the instances */
	ip->load1 = ip->load15 = -1;
	for (i = 0; i < crp->vset[LOADAV]->numval; i++) {
	    pmExtractValue(crp->vset[LOADAV]->valfmt,
			   &crp->vset[LOADAV]->vlist[i],
			   desclist[LOADAV].type, &atom, PM_TYPE_FLOAT);
	    if (crp->vset[LOADAV]->vlist[i].inst == inst1)
		ip->load1 = atom.f;
	    else if (crp->vset[LOADAV]->vlist[i].inst == inst15)
		ip->load15 = atom.f;
	}

	/* free very old result */
	pmFreeResult(prp);
    }
    ip->timestamp = crp->timestamp;

    /* swizzle result pointers */
    prp = crp;
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
    info_t		info;		/* values to report each sample */
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

    if ((sts = c = pmNewContext(opts.context, source)) < 0) {
	if (opts.context == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmGetProgname(), source, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), source, pmErrStr(sts));
	exit(1);
    }

    /* complete TZ and time window option (origin) setup */
    if (pmGetContextOptions(c, &opts)) {
	pmflush();
	exit(1);
    }

    host = pmGetContextHostName(c);
    ncpu = get_ncpu();

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

    /* prime the results */
    if (opts.context == PM_CONTEXT_ARCHIVE) {
	get_sample(&info);	/* preamble record */
	get_sample(&info);	/* and into the real data */
    }
    else
	get_sample(&info);	/* first pmFetch will prime the results */

    /* set sampling loop termination via the command line options */
    samples = opts.samples ? opts.samples : -1;

    while (samples == -1 || samples-- > 0) {
	if (lines % 15 == 0) {
	    time_t	time;
	    if (opts.context == PM_CONTEXT_ARCHIVE)
		printf("Archive: %s, ", opts.archives[0]);
	    time = info.timestamp.tv_sec;
	    printf("Host: %s, %d cpu(s), %s",
		    host, ncpu,
		    pmCtime(&time, timebuf));
/* - report format
  CPU  Busy    Busy  Free Mem   Disk     Load Average
 Util   CPU    Util  (Mbytes)   IOPS    1 Min  15 Min
X.XXX   XXX   X.XXX XXXXX.XXX XXXXXX  XXXX.XX XXXX.XX
*/
	    printf("  CPU");
	    if (ncpu > 1)
		printf("  Busy    Busy");
	    printf("  Free Mem   Disk     Load Average\n");
	    printf(" Util");
	    if (ncpu > 1)
		printf("   CPU    Util");
	    printf("  (Mbytes)   IOPS    1 Min  15 Min\n");
	}
	if (opts.context != PM_CONTEXT_ARCHIVE || pauseFlag)
	    timeval_sleep(opts.interval);
	get_sample(&info);
	if (info.cpu_util >= 0)
	    printf("%5.2f", info.cpu_util);
	else
	    printf("%5.5s", "?");
	if (ncpu > 1) {
	    if (info.peak_cpu >= 0)
		printf("   %3d", info.peak_cpu);
	    else
		printf("   %3.3s", "?");
	    if (info.peak_cpu_util >= 0)
		printf("   %5.2f", info.peak_cpu_util);
	    else
		printf("   %5.5s", "?");
	}
	if (info.freemem >= 0)
	    printf(" %9.3f", info.freemem);
	else
	    printf(" %9.9s", "?");
	if (info.dkiops >= 0)
	    printf(" %6d", info.dkiops);
	else
	    printf(" %6.6s", "?");
	if (info.load1 >= 0)
	    printf("  %7.2f", info.load1);
	else
	    printf("  %7.7s", "?");
	if (info.load15 >= 0)
	    printf("  %7.2f\n", info.load15);
	else
	    printf("  %7.7s\n", "?");
 	lines++;
    }
    exit(0);
}
