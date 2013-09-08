/*
 * pmclient - sample, simple PMAPI client
 *
 * Copyright (c) 2013 Red Hat.
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

/*
 * real time difference, *ap minus *bp
 */
double
tv_sub(struct timeval *ap, struct timeval *bp)
{
     return ap->tv_sec - bp->tv_sec + (double)(ap->tv_usec - bp->tv_usec)/1000000.0;
}

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
	
	dt = tv_sub(&crp->timestamp, &prp->timestamp);
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
    int			errflag = 0;
    int			type = 0;
    char		*host = NULL;		/* initialize to pander to gcc */
    char		*archive = NULL;
    char		*pmnsfile = PM_NS_DEFAULT;
    int			samples = -1;		/* number of samples */
    struct timeval	delta = { 5, 0 };	/* initial interval (seconds) */
    int			pauseFlag=0;
    double		skipSeconds = 0.0;
    info_t		info;		/* values to report each sample */
    int			lines = 0;
    pmLogLabel		label;			/* get hostname for archives */
    int			zflag = 0;		/* for -z */
    char 		*tz = NULL;		/* for -Z timezone */
    int			tzh;			/* initial timezone handle */
    char		timebuf[26];		/* for pmCtime result */
    char		*endnum;
    char		*msg;			/* error message */

    __pmSetProgname(argv[0]);
    setlinebuf(stdout);

    while ((c = getopt(argc, argv, "a:D:h:n:ps:S:t:zZ:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n", pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n",
			pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'p':	/* pause between updates when replaying an archive */
	    pauseFlag++;
	    break;


	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0.0) {
		fprintf(stderr, "%s: -s requires numeric argument\n",
			pmProgname);
		errflag++;
	    }
	    break;

	case 'S':	/* skip from start of archive */
	    skipSeconds = strtod(optarg, &endnum);
	    if (*endnum != '\0' || skipSeconds <= 0.0) {
		fprintf(stderr, "%s: -S requires positive numeric argument\n",
			pmProgname);
		errflag++;
	    }
	    break;

	case 't':       /* interval between samples */
	    if ((sts = pmParseInterval(optarg, &delta, &msg)) < 0) {
		fprintf(stderr, "%s: illegal -t argument\n%s\n",
			pmProgname, msg);
		errflag++;
	    }
	    break;

	case 'z':	/* timezone from host */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		errflag++;
	    }
	    zflag++;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
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

    if (zflag && type == 0) {
	fprintf(stderr, "%s: -z requires an explicit -a, or -h option\n",
		pmProgname);
	errflag++;
    }

    if (skipSeconds > 0.0 && (type == 0 || type == PM_CONTEXT_HOST)) {
	fprintf(stderr, "%s: -S can only be used with -a\n", pmProgname);
	errflag++;
    }

    if (pauseFlag && (type == 0 || type == PM_CONTEXT_HOST)) {
	fprintf(stderr, "%s: -p can only be used with -a\n", pmProgname);
	errflag++;
    }

    if (errflag || optind < argc-1) {
	fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options\n\
  -a   archive	  metrics source is a PCP log archive\n\
  -h   host       metrics source is PMCD on host\n\
  -n   pmnsfile   use an alternative PMNS\n\
  -p              pause between samples for PCP log archive\n\
  -S   numsec	  skip numsec seconds from start of PCP log archive\n\
  -s   samples	  terminate after this many samples\n\
  -t   interval	  sample interval [default 5 seconds]\n\
  -Z   timezone   set reporting timezone\n\
  -z              set reporting timezone to local time of metrics source\n",
		pmProgname);
	exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	    printf("%s: Cannot load namespace from \"%s\": %s\n",
		    pmProgname, pmnsfile, pmErrStr(sts));
	    exit(1);
	}
    }

    if (type == 0) {
	type = PM_CONTEXT_HOST;
	host = "local:";
    }
    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	archive = host;
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
	host = strdup(label.ll_hostname);
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmProgname, pmErrStr(tzh));
	    exit(1);
	}
	if (type == PM_CONTEXT_ARCHIVE)
	    printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n", label.ll_hostname);
	else
	    printf("Note: timezone set to local timezone of host \"%s\"\n\n",
		    host);
    }
    else if (tz != NULL) {
	if ((tzh = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmProgname, tz, pmErrStr(tzh));
	    exit(1);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }

    ncpu = get_ncpu();

    if (skipSeconds > 0.0 && type == PM_CONTEXT_ARCHIVE) {
	label.ll_start.tv_sec += (int)skipSeconds;
	if ((sts = pmSetMode(PM_MODE_FORW, &label.ll_start, 0)) < 0) {
	    fprintf(stderr, "%s: warning, can't skip %.1f seconds forward in archive: %s\n", pmProgname, skipSeconds, pmErrStr(sts));
	    /* don't exit */
	}
    }

    get_sample(&info);

    while (samples == -1 || samples-- > 0) {
	if (lines % 15 == 0) {
	    if (archive != NULL)
		printf("Archive: %s, ", archive);
	    printf("Host: %s, %d cpu(s), %s",
		    host, ncpu,
		    pmCtime(&info.timestamp.tv_sec, timebuf));
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
	if (type != PM_CONTEXT_ARCHIVE || pauseFlag)
	    __pmtimevalSleep(delta);
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
