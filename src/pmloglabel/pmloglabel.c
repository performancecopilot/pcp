/*
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
#include "impl.h"

static int gold;	/* boolean flag - do we have a golden label yet? */
static char *goldfile;
static __pmLogLabel golden;
static __pmLogCtl logctl;
static int status;

/*
 * Basic log control label sanity testing, with prefix/suffix len
 * checks too (these are stored as int's around the actual label).
 */
int
verify_label(FILE *f, const char *file)
{
    int version, magic;
    int n, len, xpectlen = sizeof(__pmLogLabel) + 2 * sizeof(len);

    /* check the prefix integer */
    fseek(f, (long)0, SEEK_SET);
    n = (int)fread(&len, 1, sizeof(len), f);
    len = ntohl(len);
    if (n != sizeof(len)) {
	if (feof(f)) {
	    fprintf(stderr, "Bad prefix sentinel read for %s: file too short\n",
			file);
	    status = 2;
	}
	else if (ferror(f)) {
	    fprintf(stderr, "Prefix sentinel read error for %s: %s\n",
			file, osstrerror());
	    status = 2;
	}
	else {
	    fprintf(stderr, "Prefix sentinel read error for %s: read only %d\n",
			file, n);
	    status = 2;
	}
    }
    if (len != xpectlen) {
	fprintf(stderr, "Bad prefix sentinel value for %s: %d (%d expected)\n",
			file, len, xpectlen);
	status = 2;
    }

    /* check the suffix integer */
    fseek(f, (long)(xpectlen - sizeof(len)), SEEK_SET);
    n = (int)fread(&len, 1, sizeof(len), f);
    len = ntohl(len);
    if (n != sizeof(len)) {
	if (feof(f)) {
	    fprintf(stderr, "Bad suffix sentinel read for %s: file too short\n",
			file);
	    status = 2;
	}
	else if (ferror(f)) {
	    fprintf(stderr, "Suffix sentinel read error for %s: %s\n",
			file, osstrerror());
	    status = 2;
	}
	else {
	    fprintf(stderr, "Suffix sentinel read error for %s: read only %d\n",
			file, n);
	    status = 2;
	}
    }
    if (len != xpectlen) {
	fprintf(stderr, "Bad suffix sentinel value for %s: %d (%d expected)\n",
			file, len, xpectlen);
	status = 2;
    }

    /* check the label itself */
    magic = logctl.l_label.ill_magic & 0xffffff00;
    version = logctl.l_label.ill_magic & 0xff;
    if (magic != PM_LOG_MAGIC) {
	fprintf(stderr, "Bad magic (%x) in %s\n", magic, file);
	status = 2;
    }
    if (version != PM_LOG_VERS01 && version != PM_LOG_VERS02) {
	fprintf(stderr, "Bad version (%x) in %s\n", version, file);
	status = 2;
    }

    return version;
}

/*
 * Check log control label with the known good "golden" label, if
 * we have it yet.  Passed in status is __pmLogChkLabel result, &
 * we only use that to determine if this is good as a gold label.
 */
void
compare_golden(FILE *f, const char *file, int sts, int warnings)
{
    __pmLogLabel *label = &logctl.l_label;

    if (!gold) {
	memcpy(&golden, label, sizeof(golden));
	if ((gold = (sts >= 0)) != 0)
	    goldfile = strdup(file);
    }
    else if (warnings) {
	int version = verify_label(f, file);

	if (version != (golden.ill_magic & 0xff)) {
	    fprintf(stderr, "Mismatched version (%x/%x) between %s and %s\n",
			    version, golden.ill_magic & 0xff, file, goldfile);
	    status = 2;
	}
	if (label->ill_pid != golden.ill_pid) {
	    fprintf(stderr, "Mismatched PID (%d/%d) between %s and %s\n",
			    label->ill_pid, golden.ill_pid, file, goldfile);
	    status = 2;
	}
	if (strncmp(label->ill_hostname, golden.ill_hostname,
			PM_LOG_MAXHOSTLEN) != 0) {
	    fprintf(stderr, "Mismatched hostname (%s/%s) between %s and %s\n",
		    label->ill_hostname, golden.ill_hostname, file, goldfile);
	    status = 2;
	}
	if (strncmp(label->ill_tz, golden.ill_tz, PM_TZ_MAXLEN) != 0) {
	    fprintf(stderr, "Mismatched timezone (%s/%s) between %s and %s\n",
		    label->ill_tz, golden.ill_tz, file, goldfile);
	    status = 2;
	}
    }
}

int
main(int argc, char *argv[])
{
    int			c;
    int			sts;
    int			lflag = 0;
    int			Lflag = 0;
    int			sflag = 0;
    int			errflag = 0;
    int			verbose = 0;
    int			version = 0;
    int			readonly = 1;
    int			warnings = 1;
    int			pid = 0;
    char		*archive;
    char 		*tz = NULL;
    char 		*host = NULL;
    char		buffer[MAXPATHLEN];

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:lLp:svV:Z:?")) != EOF) {
	switch (c) {
	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr,
			"%s: unrecognized debug flag specification (%s)\n",
			pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'h':	/* rewrite hostname */
	    host = optarg;
	    readonly = 0;
	    break;

	case 'l':	/* dump label */
	    lflag = 1;
	    break;

	case 'L':	/* dump label (verbose) */
	    Lflag = 1;
	    break;

	case 'p':	/* rewrite pid */
	    pid = atoi(optarg);
	    readonly = 0;
	    break;

	case 's':	/* rewrite sentinels */
	    sflag = 1;
	    readonly = 0;
	    break;

	case 'v':	/* verbose */
	    verbose = 1;
	    break;

	case 'V':	/* reset magic and version numbers */
	    version = atoi(optarg);
	    if (version != PM_LOG_VERS01 && version != PM_LOG_VERS02) {
		fprintf(stderr, "%s: unknown version number (%s)\n",
			pmProgname, optarg);
		errflag++;
	    }
	    readonly = 0;
	    break;

	case 'Z':	/* $TZ timezone */
	    tz = optarg;
	    readonly = 0;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc - 1) {
	fprintf(stderr,
"Usage: %s [options] archive\n"
"\n"
"Options:\n"
"  -h hostname  set the hostname for all files in archive\n"
"  -l           dump the archive label\n"
"  -L           more verbose form of -l\n"
"  -p pid       set the logger process ID field for all files in archive\n"
"  -s           write the label sentinel values for all files in archive\n"
"  -v           run in verbose mode, reporting on each stage of checking\n"
"  -V version   write magic and version numbers for all files in archive\n"
"  -Z timezone  set the timezone for all files in archive\n",
		pmProgname);
	exit(1);
    }

    archive = argv[optind];
    warnings = (readonly || verbose);

    if (verbose)
	printf("Scanning for components of archive \"%s\"\n", archive);
    if ((sts = __pmLogLoadLabel(&logctl, archive)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, archive, pmErrStr(sts));
	exit(1);
    }

    logctl.l_curvol = -1;
    logctl.l_physend = -1;

    /*
     * Read the label from each data volume, check, and report status
     */
    for (c = logctl.l_minvol; c <= logctl.l_maxvol; c++) {
	if (verbose)
	    printf("Checking label on data volume %d\n", c);
	if ((sts = __pmLogChangeVol(&logctl, c)) < 0 && warnings) {
	    fprintf(stderr, "Bad data volume %d label: %s\n", c, pmErrStr(sts));
	    status = 2;
	}
	snprintf(buffer, sizeof(buffer), "data volume %d", c);
	compare_golden(logctl.l_mfp, buffer, sts, warnings);
    }

    if (logctl.l_tifp) {
	if (verbose)
	    printf("Checking label on temporal index\n");
	if ((sts = __pmLogChkLabel(&logctl, logctl.l_tifp, &logctl.l_label,
			           PM_LOG_VOL_TI)) < 0 && warnings) {
	    fprintf(stderr, "Bad temporal index label: %s\n", pmErrStr(sts));
	    status = 2;
	}
	compare_golden(logctl.l_tifp, "temporal index", sts, warnings);
    }
    else if (verbose) {
	printf("No temporal index found\n");
    }

    if (verbose)
	printf("Checking label on metadata volume\n");
    if ((sts = __pmLogChkLabel(&logctl, logctl.l_mdfp, &logctl.l_label,
			       PM_LOG_VOL_META)) < 0 && warnings) {
	fprintf(stderr, "Bad metadata volume label: %s\n", pmErrStr(sts));
	status = 2;
    }
    compare_golden(logctl.l_mdfp, "metadata volume", sts, warnings);

    /*
     * Now, make any modifications requested
     */
    if (!readonly) {
	if (version)
	    golden.ill_magic = PM_LOG_MAGIC | version;
	if (pid)
	    golden.ill_pid = pid;
	if (host) {
	    memset(golden.ill_hostname, 0, sizeof(golden.ill_hostname));
	    strncpy(golden.ill_hostname, host, PM_LOG_MAXHOSTLEN-1);
	    golden.ill_hostname[PM_LOG_MAXHOSTLEN-1] = '\0';
	}
	if (tz) {
	    memset(golden.ill_tz, 0, sizeof(golden.ill_tz));
	    strncpy(golden.ill_tz, tz, PM_TZ_MAXLEN-1);
	    golden.ill_tz[PM_TZ_MAXLEN-1] = '\0';
	}

	if (logctl.l_mfp)
	    fclose(logctl.l_mfp);
	for (c = logctl.l_minvol; c <= logctl.l_maxvol; c++) {
	    if (verbose)
		printf("Writing label on data volume %d\n", c);
	    golden.ill_vol = c;
	    snprintf(buffer, sizeof(buffer), "%s.%d", logctl.l_name, c);
	    if ((logctl.l_mfp = fopen(buffer, "r+")) == NULL) {
		fprintf(stderr, "Failed data volume %d open: %s\n",
				c, osstrerror());
		status = 3;
	    }
	    else if ((sts = __pmLogWriteLabel(logctl.l_mfp, &golden)) < 0) {
		fprintf(stderr, "Failed data volume %d label write: %s\n",
				c, pmErrStr(sts));
		status = 3;
	    }
	    if (logctl.l_mfp)
		fclose(logctl.l_mfp);
	}
	/* Need to reset the data volume, for subsequent label read */
	logctl.l_mfp = NULL;
	logctl.l_curvol = -1;
	__pmLogChangeVol(&logctl, logctl.l_minvol);

	if (logctl.l_tifp) {
	    fclose(logctl.l_tifp);
	    if (verbose)
		printf("Writing label on temporal index\n");
	    golden.ill_vol = PM_LOG_VOL_TI;
	    snprintf(buffer, sizeof(buffer), "%s.index", logctl.l_name);
	    if ((logctl.l_tifp = fopen(buffer, "r+")) == NULL) {
		fprintf(stderr, "Failed temporal index open: %s\n",
				osstrerror());
		status = 3;
	    }
	    else if ((sts = __pmLogWriteLabel(logctl.l_tifp, &golden)) < 0) {
		fprintf(stderr, "Failed temporal index label write: %s\n",
				pmErrStr(sts));
		status = 3;
	    }
	}

	fclose(logctl.l_mdfp);
	if (verbose)
	    printf("Writing label on metadata volume\n");
	golden.ill_vol = PM_LOG_VOL_META;
	snprintf(buffer, sizeof(buffer), "%s.meta", logctl.l_name);
	if ((logctl.l_mdfp = fopen(buffer, "r+")) == NULL) {
	    fprintf(stderr, "Failed metadata volume open: %s\n",
			    osstrerror());
	    status = 3;
	}
	else if ((sts = __pmLogWriteLabel(logctl.l_mdfp, &golden)) < 0) {
	    fprintf(stderr, "Failed metadata volume label write: %s\n",
			    pmErrStr(sts));
	    status = 3;
	}
    }

    /*
     * Finally, dump out the label if requested
     */
    if (lflag || Lflag) {
	char	       *ddmm;
	char	       *yr;
	struct timeval	tv;
	time_t t = golden.ill_start.tv_sec;

	printf("Log Label (Log Format Version %d)\n", golden.ill_magic & 0xff);
	printf("Performance metrics from host %s\n", golden.ill_hostname);

	ddmm = pmCtime(&t, buffer);
	ddmm[10] = '\0';
	yr = &ddmm[20];
	printf("  commencing %s ", ddmm);
	tv.tv_sec = golden.ill_start.tv_sec;
	tv.tv_usec = golden.ill_start.tv_usec;
	__pmPrintStamp(stdout, &tv);
	printf(" %4.4s\n", yr);
	if (__pmLogChangeVol(&logctl, 0) < 0)
	    printf("  ending     UNKNOWN\n");
	else if (__pmGetArchiveEnd(&logctl, &tv) < 0)
	    printf("  ending     UNKNOWN\n");
	else {
	    ddmm = pmCtime(&tv.tv_sec, buffer);
	    ddmm[10] = '\0';
	    yr = &ddmm[20];
	    printf("  ending     %s ", ddmm);
	    __pmPrintStamp(stdout, &tv);
	    printf(" %4.4s\n", yr);
	}
	if (Lflag) {
	    printf("Archive timezone: %s\n", golden.ill_tz);
	    printf("PID for pmlogger: %d\n", golden.ill_pid);
	}
    }

    exit(status);
}
