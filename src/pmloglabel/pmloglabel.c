/*
 * Copyright (c) 2014-2017,2021 Red Hat.
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
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
#include "libpcp.h"

static int gold;	/* boolean flag - do we have a golden label yet? */
static char *goldfile;
static __pmLogLabel golden;
static __pmLogCtl logctl;
static __pmArchCtl archctl;
static int status;

/*
 * Basic log control label sanity testing, with header/trailer len
 * checks too (these are stored as int's around the actual label).
 */

int
verify_label(__pmFILE *f, const char *file, __pmLogLabel *lp)
{
    int		sts;

    if (gold)
	__pmLogFreeLabel(lp);
    if ((sts = __pmLogLoadLabel(f, lp)) < 0) {
	fprintf(stderr, "%s: cannot load label record: %s\n", file, pmErrStr(sts));
	return -1;
    }

    return __pmLogVersion(&logctl);
}

/*
 * Check log control label with the known good "golden" label, if
 * we have it yet.  Passed in status is __pmLogChkLabel result, &
 * we only use that to determine if this is good as a gold label.
 */
void
compare_golden(__pmFILE *f, const char *file, int sts, int warnings)
{
    __pmLogLabel *label = &logctl.label;

    if (!gold) {
	memcpy(&golden, label, sizeof(golden));
	if ((gold = (sts >= 0)) != 0)
	    goldfile = strdup(file);
	memset(label, 0, sizeof(*label));	/* avoid double freeing */
    }
    else if (warnings) {
	int version = verify_label(f, file, label);

	if (version < 0)
	    return;	/* no label content was successfully read */

	if (version != (golden.magic & 0xff)) {
	    fprintf(stderr, "Mismatched version (%x/%x) between %s and %s\n",
			    version, golden.magic & 0xff, file, goldfile);
	    status = 2;
	}
	if (label->pid != golden.pid) {
	    fprintf(stderr, "Mismatched PID (%d/%d) between %s and %s\n",
			    label->pid, golden.pid, file, goldfile);
	    status = 2;
	}
	if (!label->hostname || !golden.hostname ||
	    strcmp(label->hostname, golden.hostname) != 0) {
	    fprintf(stderr, "Mismatched hostname (%s/%s) between %s and %s\n",
		    label->hostname ? label->hostname : "\"\"",
		    golden.hostname ? golden.hostname : "\"\"", file, goldfile);
	    status = 2;
	}
	if (!label->timezone || !golden.timezone ||
	    strcmp(label->timezone, golden.timezone) != 0) {
	    fprintf(stderr, "Mismatched timezone (%s/%s) between %s and %s\n",
		    label->timezone ? label->timezone : "\"\"",
		    golden.timezone ? golden.timezone : "\"\"", file, goldfile);
	    status = 2;
	}
	if (label->zoneinfo && golden.zoneinfo &&	/* optional */
	    strcmp(label->zoneinfo, golden.zoneinfo) != 0) {
	    fprintf(stderr, "Mismatched zoneinfo (%s/%s) between %s and %s\n",
		    label->zoneinfo ? label->zoneinfo : "\"\"",
		    golden.zoneinfo ? golden.zoneinfo : "\"\"", file, goldfile);
	    status = 2;
	}
    }
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "host", 1, 'h', "HOSTNAME", "set the hostname for all files in archive" },
    { "label", 0, 'l', 0, "dump the archive label" },
    { "", 0, 'L', 0, "more verbose form of label dump" },
    { "pid", 1, 'p', "PID", "set the logger process ID field for all files in archive" },
    { "", 0, 's', 0, "write the label sentinel values for all files in archive" },
    { "verbose", 0, 'v', 0, "run in verbose mode, reporting on each stage of checking" },
    { "version", 1, 'V', "NUM", "write magic and version numbers for all files in archive" },
    { "timezone", 1, 'Z', "TZ", "set the timezone for all files in archive" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:h:lLp:svV:Z:?",
    .long_options = longopts,
    .short_usage = "[options] archive",
};

int
main(int argc, char *argv[])
{
    int			c;
    int			sts;
    int			lflag = 0;
    int			Lflag = 0;
    int			verbose = 0;
    int			version = 0;
    int			readonly = 1;
    int			warnings = 1;
    int			pid = 0;
    char		*archive;
    char 		*tz = NULL;
    char 		*host = NULL;
    char		buffer[MAXPATHLEN];

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'D':	/* debug options */
	    sts = pmSetDebug(opts.optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug options specification (%s)\n",
			pmGetProgname(), opts.optarg);
		opts.errors++;
	    }
	    break;

	case 'h':	/* rewrite hostname */
	    host = opts.optarg;
	    readonly = 0;
	    break;

	case 'l':	/* dump label */
	    lflag = 1;
	    break;

	case 'L':	/* dump label (verbose) */
	    Lflag = 1;
	    break;

	case 'p':	/* rewrite pid */
	    pid = atoi(opts.optarg);
	    readonly = 0;
	    break;

	case 's':	/* rewrite sentinels */
	    readonly = 0;
	    break;

	case 'v':	/* verbose */
	    verbose = 1;
	    break;

	case 'V':	/* reset magic and version numbers */
	    version = atoi(opts.optarg);
	    if (version != PM_LOG_VERS02 && version != PM_LOG_VERS03) {
		fprintf(stderr, "%s: unknown version number (%s)\n",
			pmGetProgname(), opts.optarg);
		opts.errors++;
	    }
	    readonly = 0;
	    break;

	case 'Z':	/* $TZ timezone */
	    tz = opts.optarg;
	    readonly = 0;
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.optind != argc - 1 && opts.narchives == 0) {
	pmprintf("%s: insufficient arguments\n", pmGetProgname());
	opts.errors++;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    archive = opts.narchives > 0? opts.archives[0] : argv[opts.optind];
    warnings = (readonly || verbose);

    archctl.ac_log = &logctl;

    if (verbose)
	printf("Scanning for components of archive \"%s\"\n", archive);
    if ((sts = __pmLogFindOpen(&archctl, archive)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmGetProgname(), archive, pmErrStr(sts));
	exit(1);
    }

    archctl.ac_curvol = -1;
    logctl.physend = -1;

    /*
     * Read the label from each data volume, check, and report status
     */
    for (c = logctl.minvol; c <= logctl.maxvol; c++) {
	if (verbose)
	    printf("Checking label on data volume %d\n", c);
	if ((sts = __pmLogChangeVol(&archctl, c)) < 0 && warnings) {
	    fprintf(stderr, "Bad data volume %d label: %s\n", c, pmErrStr(sts));
	    status = 2;
	}
	pmsprintf(buffer, sizeof(buffer), "data volume %d", c);
	compare_golden(archctl.ac_mfp, buffer, sts, warnings);
	__pmLogFreeLabel(&logctl.label);
    }

    if (logctl.tifp) {
	if (verbose)
	    printf("Checking label on temporal index\n");
	if ((sts = __pmLogChkLabel(&archctl, logctl.tifp, &logctl.label,
			           PM_LOG_VOL_TI)) < 0 && warnings) {
	    fprintf(stderr, "Bad temporal index label: %s\n", pmErrStr(sts));
	    status = 2;
	}
	compare_golden(logctl.tifp, "temporal index", sts, warnings);
	__pmLogFreeLabel(&logctl.label);
    }
    else if (verbose) {
	printf("No temporal index found\n");
    }

    if (verbose)
	printf("Checking label on metadata volume\n");
    if ((sts = __pmLogChkLabel(&archctl, logctl.mdfp, &logctl.label,
			       PM_LOG_VOL_META)) < 0 && warnings) {
	fprintf(stderr, "Bad metadata volume label: %s\n", pmErrStr(sts));
	status = 2;
    }
    compare_golden(logctl.mdfp, "metadata volume", sts, warnings);
    __pmLogFreeLabel(&logctl.label);

    /*
     * Now, make any modifications requested
     */
    if (!readonly) {
	if (version)
	    golden.magic = PM_LOG_MAGIC | version;
	if (pid)
	    golden.pid = pid;
	if (host) {
	    free(golden.hostname);
	    golden.hostname = strdup(host);
	}
	if (tz) {
	    free(golden.timezone);
	    golden.timezone = strdup(tz);
	}
	/* TODO: v3 archive zoneinfo */
	golden.zoneinfo = NULL;

	if (archctl.ac_mfp)
	    __pmFclose(archctl.ac_mfp);
	for (c = logctl.minvol; c <= logctl.maxvol; c++) {
	    if (verbose)
		printf("Writing label on data volume %d\n", c);
	    golden.vol = c;
	    pmsprintf(buffer, sizeof(buffer), "%s.%d", logctl.name, c);
	    if ((archctl.ac_mfp = __pmFopen(buffer, "r+")) == NULL) {
		fprintf(stderr, "Failed data volume %d open: %s\n",
				c, osstrerror());
		status = 3;
	    }
	    else if ((sts = __pmLogWriteLabel(archctl.ac_mfp, &golden)) < 0) {
		fprintf(stderr, "Failed data volume %d label write: %s\n",
				c, pmErrStr(sts));
		status = 3;
	    }
	    if (archctl.ac_mfp)
		__pmFclose(archctl.ac_mfp);
	}
	/* Need to reset the data volume, for subsequent label read */
	archctl.ac_mfp = NULL;
	archctl.ac_curvol = -1;
	__pmLogChangeVol(&archctl, logctl.minvol);

	if (logctl.tifp) {
	    __pmFclose(logctl.tifp);
	    if (verbose)
		printf("Writing label on temporal index\n");
	    golden.vol = PM_LOG_VOL_TI;
	    pmsprintf(buffer, sizeof(buffer), "%s.index", logctl.name);
	    if ((logctl.tifp = __pmFopen(buffer, "r+")) == NULL) {
		fprintf(stderr, "Failed temporal index open: %s\n",
				osstrerror());
		status = 3;
	    }
	    else if ((sts = __pmLogWriteLabel(logctl.tifp, &golden)) < 0) {
		fprintf(stderr, "Failed temporal index label write: %s\n",
				pmErrStr(sts));
		status = 3;
	    }
	    if (logctl.tifp)
		__pmFclose(logctl.tifp);
	}

	__pmFclose(logctl.mdfp);
	if (verbose)
	    printf("Writing label on metadata volume\n");
	golden.vol = PM_LOG_VOL_META;
	pmsprintf(buffer, sizeof(buffer), "%s.meta", logctl.name);
	if ((logctl.mdfp = __pmFopen(buffer, "r+")) == NULL) {
	    fprintf(stderr, "Failed metadata volume open: %s\n",
			    osstrerror());
	    status = 3;
	}
	else if ((sts = __pmLogWriteLabel(logctl.mdfp, &golden)) < 0) {
	    fprintf(stderr, "Failed metadata volume label write: %s\n",
			    pmErrStr(sts));
	    status = 3;
	}
	if (logctl.mdfp)
	    __pmFclose(logctl.mdfp);
    }

    /*
     * Finally, dump out the label if requested
     */
    if (lflag || Lflag) {
	char	       *ddmm;
	char	       *yr;
	struct timespec	ts;
	time_t t = golden.start.sec;

	printf("Log Label (Log Format Version %d)\n", golden.magic & 0xff);
	printf("Performance metrics from host %s\n", golden.hostname);

	ddmm = pmCtime(&t, buffer);
	ddmm[10] = '\0';
	yr = &ddmm[20];
	printf("  commencing %s ", ddmm);
	__pmPrintTimestamp(stdout, &golden.start);
	printf(" %4.4s\n", yr);
	if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0)
	    printf("  ending     UNKNOWN (pmNewContext: %s)\n", pmErrStr(sts));
	else if ((sts = pmGetArchiveEnd(&ts)) < 0)
	    printf("  ending     UNKNOWN (pmGetArchiveEnd: %s)\n", pmErrStr(sts));
	else {
	    time_t	time;
	    time = ts.tv_sec;
	    ddmm = pmCtime(&time, buffer);
	    ddmm[10] = '\0';
	    yr = &ddmm[20];
	    printf("  ending     %s ", ddmm);
	    pmtimespecPrint(stdout, &ts);
	    printf(" %4.4s\n", yr);
	}
	if (Lflag) {	/* TODO: v3 archives - zoneinfo */
	    printf("Archive timezone: %s\n", golden.timezone);
	    printf("PID for pmlogger: %d\n", golden.pid);
	}
    }

    exit(status);
}
