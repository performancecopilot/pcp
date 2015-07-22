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
#include "impl.h"
#include "pmnsmap.h"

pmLongOptions longopts[] = {
    PMAPI_GENERAL_OPTIONS,
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "pause", 0, 'P', 0, "pause between updates for archive replay" },
    PMAPI_OPTIONS_END
};

pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ,
    .short_options = PMAPI_OPTIONS "P",
    .long_options = longopts,
};

typedef struct {
    struct timeval	timestamp;	/* last fetched time */
    float		cpu_util;	/* aggregate CPU utilization, usr+sys */
    int			peak_cpu;	/* most utilized CPU, if > 1 CPU */
    float		peak_cpu_util;	/* utilization for most utilized CPU */
    float		freemem;	/* free memory (Mbytes) */
    unsigned int	dkiops;		/* aggregate disk I/O's per second */
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

    if ((sts = pmLookupName(1, pmclient_init, pmidlist)) < 0) {
	fprintf(stderr, "%s: pmLookupName: %s\n", pmProgname, pmErrStr(sts));
	fprintf(stderr, "%s: metric \"%s\" not in name space\n",
			pmProgname, pmclient_init[0]);
	exit(1);
    }
    if ((sts = pmLookupDesc(pmidlist[0], desclist)) < 0) {
	fprintf(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
		pmProgname, pmclient_init[0], pmIDStr(pmidlist[0]), pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetch(1, pmidlist, &rp)) < 0) {
	fprintf(stderr, "%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
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
	    fprintf(stderr, "%s: get_sample: malloc: %s\n", pmProgname, osstrerror());
	    exit(1);
	}
	if ((desclist = (pmDesc *)malloc(numpmid * sizeof(desclist[0]))) == NULL) {
	    fprintf(stderr, "%s: get_sample: malloc: %s\n", pmProgname, osstrerror());
	    exit(1);
	}
	if ((sts = pmLookupName(numpmid, pmclient_sample, pmidlist)) < 0) {
	    printf("%s: pmLookupName: %s\n", pmProgname, pmErrStr(sts));
	    for (i = 0; i < numpmid; i++) {
		if (pmidlist[i] == PM_ID_NULL)
		    fprintf(stderr, "%s: metric \"%s\" not in name space\n", pmProgname, pmclient_sample[i]);
	    }
	    exit(1);
	}
	for (i = 0; i < numpmid; i++) {
	    if ((sts = pmLookupDesc(pmidlist[i], &desclist[i])) < 0) {
		fprintf(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
		    pmProgname, pmclient_sample[i], pmIDStr(pmidlist[i]), pmErrStr(sts));
		exit(1);
	    }
	}
    }

    /* fetch the current metrics */
    if ((sts = pmFetch(numpmid, pmidlist, &crp)) < 0) {
	fprintf(stderr, "%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    /*
     * minor gotcha ... for archives, it helps to do the first fetch of
     * real data before interrogating the instance domains ... this
     * forces us to be "after" the first batch of instance domain info
     * in the meta data files
     */
    if (first) {
	/*
	 * from now on, just want the 1 minute and 15 minute load averages,
	 * so limit the instance profile for this metric
	 */
	pmDelProfile(desclist[LOADAV].indom, 0, NULL);	/* all off */
	if ((inst1 = pmLookupInDom(desclist[LOADAV].indom, "1 minute")) < 0) {
	    fprintf(stderr, "%s: cannot translate instance for 1 minute load average\n", pmProgname);
	    exit(1);
	}
	pmAddProfile(desclist[LOADAV].indom, 1, &inst1);
	if ((inst15 = pmLookupInDom(desclist[LOADAV].indom, "15 minute")) < 0) {
	    fprintf(stderr, "%s: cannot translate instance for 15 minute load average\n", pmProgname);
	    exit(1);
	}
	pmAddProfile(desclist[LOADAV].indom, 1, &inst15);

	first = 0;
    }

    /* if the second or later sample, pick the results apart */
    if (prp !=  NULL) {
	
	dt = __pmtimevalSub(&crp->timestamp, &prp->timestamp);
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

	/* freemem - expect just one value */
	pmExtractValue(crp->vset[FREEMEM]->valfmt, crp->vset[FREEMEM]->vlist,
		    desclist[FREEMEM].type, &tmp, PM_TYPE_FLOAT);
	/* convert from today's units at the collection site to Mbytes */
	pmConvScale(PM_TYPE_FLOAT, &tmp, &desclist[FREEMEM].units,
		    &atom, &mbyte_scale);
	ip->freemem = atom.f;

	/* disk IOPS - expect just one value, but need delta */
	pmExtractValue(crp->vset[DKIOPS]->valfmt, crp->vset[DKIOPS]->vlist,
		    desclist[DKIOPS].type, &atom, PM_TYPE_U32);
	ip->dkiops = atom.ul;
	pmExtractValue(prp->vset[DKIOPS]->valfmt, prp->vset[DKIOPS]->vlist,
		    desclist[DKIOPS].type, &atom, PM_TYPE_U32);
	ip->dkiops -= atom.ul;
	ip->dkiops = ((float)(ip->dkiops) + 0.5) / dt;

	/* load average ... process all values, matching up the instances */
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
	case 'p':
	    pauseFlag++;
	    break;
	default:
	    opts.errors++;
	    break;
	}
    }

    if (pauseFlag && opts.context != PM_CONTEXT_ARCHIVE) {
	pmprintf("%s: pause can only be used with archives\n", pmProgname);
	opts.errors++;
    }

    if (opts.errors || opts.optind < argc - 1) {
	pmUsageMessage(&opts);
	exit(1);
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
		    pmProgname, source, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmProgname, source, pmErrStr(sts));
	exit(1);
    }

    /* complete TZ and time window option (origin) setup */
    if (pmGetContextOptions(c, &opts)) {
	pmflush();
	exit(1);
    }

    host = pmGetContextHostName(c);
    ncpu = get_ncpu();

    if ((opts.context == PM_CONTEXT_ARCHIVE) &&
	(opts.start.tv_sec != 0 || opts.start.tv_usec != 0)) {
	if ((sts = pmSetMode(PM_MODE_FORW, &opts.start, 0)) < 0) {
	    fprintf(stderr, "%s: pmSetMode failed: %s\n",
		    pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }

    get_sample(&info);

    /* set a default sampling interval if none has been requested */
    if (opts.interval.tv_sec == 0 && opts.interval.tv_usec == 0)
	opts.interval.tv_sec = 5;

    /* set sampling loop termination via the command line options */
    samples = opts.samples ? opts.samples : -1;

    while (samples == -1 || samples-- > 0) {
	if (lines % 15 == 0) {
	    if (opts.context == PM_CONTEXT_ARCHIVE)
		printf("Archive: %s, ", opts.archives[0]);
	    printf("Host: %s, %d cpu(s), %s",
		    host, ncpu,
		    pmCtime((const time_t *)&info.timestamp.tv_sec, timebuf));
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
	    __pmtimevalSleep(opts.interval);
	get_sample(&info);
	printf("%5.2f", info.cpu_util);
	if (ncpu > 1)
	    printf("   %3d   %5.2f", info.peak_cpu, info.peak_cpu_util);
	printf(" %9.3f", info.freemem);
	printf(" %6d", info.dkiops);
	printf("  %7.2f %7.2f\n", info.load1, info.load15);
 	lines++;
    }
    exit(0);
}
