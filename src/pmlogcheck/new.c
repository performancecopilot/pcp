/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 2013 Ken McDonell.
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
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>

#define STS_FATAL	-2
#define STS_WARNING	-1
#define STS_OK		0

static struct timeval	tv;
static char		*archpathname;	/* from the command line */
static char		*archbasename;	/* after basename() */
static char		*archdirname;	/* after dirname() */
#define STATE_OK	1
#define STATE_MISSING	2
#define STATE_BAD	3
static int		index_state = STATE_MISSING;
static int		meta_state = STATE_MISSING;
static int		log_state = STATE_MISSING;
static int		vflag;
static int		sep;

/*
 * check the temporal index
 */
static void
pass1(__pmContext *ctxp)
{
    int		i;
    char	path[MAXPATHLEN];
    off_t	meta_size = -1;		/* initialize to pander to gcc */
    off_t	log_size = -1;		/* initialize to pander to gcc */
    struct stat	sbuf;
    __pmLogTI	*tip;
    __pmLogTI	*lastp;

    if (vflag) printf("pass1: check temporal index\n");
    lastp = NULL;
    for (i = 1; i <= ctxp->c_archctl->ac_log->l_numti; i++) {
	/*
	 * Integrity Checks
	 *
	 * this(tv_sec) < 0
	 * this(tv_usec) < 0 || this(tv_usec) > 999999
	 * this(timestamp) < last(timestamp)
	 * this(vol) >= 0
	 * this(vol) < last(vol)
	 * this(vol) == last(vol) && this(meta) <= last(meta)
	 * this(vol) == last(vol) && this(log) <= last(log)
	 * file_exists(<base>.meta) && this(meta) > file_size(<base>.meta)
	 * file_exists(<base>.this(vol)) &&
	 *		this(log) > file_size(<base>.this(vol))
	 *
	 * Integrity Warnings
	 *
	 * this(vol) != last(vol) && !file_exists(<base>.this(vol))
	 */
	tip = &ctxp->c_archctl->ac_log->l_ti[i-1];
	tv.tv_sec = tip->ti_stamp.tv_sec;
	tv.tv_usec = tip->ti_stamp.tv_usec;
	if (i == 1) {
	    sprintf(path, "%s%c%s.meta", archdirname, sep, archbasename);
	    if (stat(path, &sbuf) == 0)
		meta_size = sbuf.st_size;
	    else {
		/* should not get here ... as detected in after pass0 */
		fprintf(stderr, "%s: pass1: Botch: cannot open metadata file (%s)\n", pmProgname, path);
		exit(1);
	    }
	}
	if (tip->ti_vol < 0) {
	    printf("%s.index[entry %d]: illegal volume number %d\n",
		    archbasename, i, tip->ti_vol);
	    index_state = STATE_BAD;
	    log_size = -1;
	}
	else if (lastp == NULL || tip->ti_vol != lastp->ti_vol) { 
	    sprintf(path, "%s%c%s.%d", archdirname, sep, archbasename, tip->ti_vol);
	    if (stat(path, &sbuf) == 0)
		log_size = sbuf.st_size;
	    else {
		log_size = -1;
		printf("%s: File missing or compressed for log volume %d\n", path, tip->ti_vol);
	    }
	}
	if (tip->ti_stamp.tv_sec < 0 ||
	    tip->ti_stamp.tv_usec < 0 || tip->ti_stamp.tv_usec > 999999) {
	    printf("%s.index[entry %d]: Illegal timestamp value (%d sec, %d usec)\n",
		archbasename, i, tip->ti_stamp.tv_sec, tip->ti_stamp.tv_usec);
	    index_state = STATE_BAD;
	}
	if (tip->ti_meta < sizeof(__pmLogLabel)+2*sizeof(int)) {
	    printf("%s.index[entry %d]: offset to metadata (%ld) before end of label record (%ld)\n",
		archbasename, i, (long)tip->ti_meta, (long)(sizeof(__pmLogLabel)+2*sizeof(int)));
	    index_state = STATE_BAD;
	}
	if (meta_size != -1 && tip->ti_meta > meta_size) {
	    printf("%s.index[entry %d]: offset to metadata (%ld) past end of file (%ld)\n",
		archbasename, i, (long)tip->ti_meta, (long)meta_size);
	    index_state = STATE_BAD;
	}
	if (tip->ti_log < sizeof(__pmLogLabel)+2*sizeof(int)) {
	    printf("%s.index[entry %d]: offset to log (%ld) before end of label record (%ld)\n",
		archbasename, i, (long)tip->ti_log, (long)(sizeof(__pmLogLabel)+2*sizeof(int)));
	    index_state = STATE_BAD;
	}
	if (log_size != -1 && tip->ti_log > log_size) {
	    printf("%s.index[entry %d]: offset to log (%ld) past end of file (%ld)\n",
		archbasename, i, (long)tip->ti_log, (long)log_size);
	    index_state = STATE_BAD;
	}
	if (lastp != NULL) {
	    if (tip->ti_stamp.tv_sec < lastp->ti_stamp.tv_sec ||
	        (tip->ti_stamp.tv_sec == lastp->ti_stamp.tv_sec &&
	         tip->ti_stamp.tv_usec < lastp->ti_stamp.tv_usec)) {
		printf("%s.index: timestamp went backwards in time %d.%06d[entry %d]-> %d.%06d[entry %d]\n",
			archbasename, (int)lastp->ti_stamp.tv_sec, (int)lastp->ti_stamp.tv_usec, i-1,
			(int)tip->ti_stamp.tv_sec, (int)tip->ti_stamp.tv_usec, i);
		index_state = STATE_BAD;
	    }
	    if (tip->ti_vol < lastp->ti_vol) {
		printf("%s.index: volume number decreased %d[entry %d] -> %d[entry %d]\n",
			archbasename, lastp->ti_vol, i-1, tip->ti_vol, i);
		index_state = STATE_BAD;
	    }
	    if (tip->ti_vol == lastp->ti_vol && tip->ti_meta < lastp->ti_meta) {
		printf("%s.index: offset to metadata decreased %ld[entry %d] -> %ld[entry %d]\n",
			archbasename, (long)lastp->ti_meta, i-1, (long)tip->ti_meta, i);
		index_state = STATE_BAD;
	    }
	    if (tip->ti_vol == lastp->ti_vol && tip->ti_log < lastp->ti_log) {
		printf("%s.index: offset to log decreased %ld[entry %d] -> %ld[entry %d]\n",
			archbasename, (long)lastp->ti_log, i-1, (long)tip->ti_log, i);
		index_state = STATE_BAD;
	    }
	}
	lastp = tip;
    }
}

static int
filter(const_dirent *dp)
{
    static int	len = -1;

    if (len == -1)
	len = strlen(archbasename);
    if (strncmp(dp->d_name, archbasename, len) != 0)
	return 0;
    if (strcmp(&dp->d_name[len], ".meta") == 0)
	return 1;
    if (strcmp(&dp->d_name[len], ".index") == 0)
	return 1;
    if (dp->d_name[len] == '.' && isdigit(dp->d_name[len+1])) {
	const char	*p = &dp->d_name[len+2];
	for ( ; *p; p++) {
	    if (!isdigit(*p))
		return 0;
	}
	return 1;
    }
    return 0;
}

#define IS_UNKNOWN	0
#define IS_INDEX	1
#define IS_META		2
#define IS_LOG		3
/*
 * Pass 0 for all files
 * - should only come here if fname exists
 * - check that file contains a number of complete records
 * - the label record and the records for the data and metadata
 *   have this format:
 *   :----------:----------------------:---------:
 *   | int len  |        stuff         | int len |
 *   | header   |        stuff         | trailer |
 *   :----------:----------------------:---------:
 *   and the len fields are in network byte order.
 *   For these records, check that the header length is equal to
 *   the trailer length
 * - for index files, following the label record there should be
 *   a number of complete records, each of which is a __pmLogTI
 *   record, with the fields converted network byte order
 *
 * TODO - repair
 * - truncate metadata and data files ... unconditional or interactive confirm?
 * - mark index as bad and needing rebuild
 * - move access check into here (if cannot open file for reading we're screwed)
 */
static int
pass0(char *fname)
{
    int		len;
    int		check;
    int		i;
    int		sts;
    int		nrec = 0;
    int		is = IS_UNKNOWN;
    char	*p;
    FILE	*f;

    if (vflag)
	printf("pass0: %s:\n", fname);
    if ((f = fopen(fname, "r")) == NULL) {
	fprintf(stderr, "%s: Cannot open \"%s\": %s\n", pmProgname, fname, osstrerror());
	sts = STS_FATAL;
	goto done;
    }
    p = strrchr(fname, '.');
    if (p != NULL) {
	if (strcmp(p, ".index") == 0)
	    is = IS_INDEX;
	else if (strcmp(p, ".meta") == 0)
	    is = IS_META;
	else if (isdigit(*++p)) {
	    p++;
	    while (*p && isdigit(*p))
		p++;
	    if (*p == '\0')
		is = IS_LOG;
	}
    }
    if (is == IS_UNKNOWN) {
	/*
	 * should never get here because filter() is supposed to
	 * only include PCP archive file names from scandir()
	 */
	fprintf(stderr, "%s: pass0: Botch: bad file name? %s\n", pmProgname, fname);
	exit(1);
    }

    while ((sts = fread(&len, 1, sizeof(len), f)) == sizeof(len)) {
	len = ntohl(len);
	len -= 2 * sizeof(len);
	/* gobble stuff between header and trailer without looking at it */
	for (i = 0; i < len; i++) {
	    check = fgetc(f);
	    if (check == EOF) {
		if (nrec == 0)
		    printf("%s: unexpected EOF in label record body\n", fname);
		else
		    printf("%s[record %d]: unexpected EOF in record body\n", fname, nrec);
		sts = STS_FATAL;
		goto done;
	    }
	}
	if ((sts = fread(&check, 1, sizeof(check), f)) != sizeof(check)) {
	    if (nrec == 0)
		printf("%s: unexpected EOF in label record trailer\n", fname);
	    else
		printf("%s[record %d]: unexpected EOF in record trailer\n", fname, nrec);
	    sts = STS_FATAL;
	    goto done;
	}
	check = ntohl(check);
	len += 2 * sizeof(len);
	if (check != len) {
	    if (nrec == 0)
		printf("%s: label record length mismatch: header %d != trailer %d\n", fname, len, check);
	    else
		printf("%s[record %d]: length mismatch: header %d != trailer %d\n", fname, nrec, len, check);
	    sts = STS_FATAL;
	    goto done;
	}
	nrec++;
	if (is == IS_INDEX) {
	    /* for index files, done label record, now eat index records */
	    __pmLogTI	tirec;
	    while ((sts = fread(&tirec, 1, sizeof(tirec), f)) == sizeof(tirec)) { 
		nrec++;
	    }
	    if (sts != 0) {
		printf("%s[record %d]: unexpected EOF in index entry\n", fname, nrec);
		index_state = STATE_BAD;
		sts = STS_FATAL;
		goto done;
	    }
	    goto empty_check;
	}
    }
    if (sts != 0) {
	printf("%s[record %d]: unexpected EOF in record header\n", fname, nrec);
	sts = STS_FATAL;
    }
empty_check:
    if (sts != STS_FATAL && nrec < 2) {
	printf("%s: contains no PCP data\n", fname);
	sts = STS_WARNING;
    }
    /*
     * sts == 0 (from fread) => STS_OK
     */
done:
    if (is == IS_INDEX) {
	if (sts == STS_OK)
	    index_state = STATE_OK;
	else
	    index_state = STATE_BAD;
    }
    else if (is == IS_META) {
	if (sts == STS_OK)
	    meta_state = STATE_OK;
	else
	    meta_state = STATE_BAD;
    }
    else {
	if (log_state == STATE_OK && sts != STS_OK)
	    log_state = STATE_BAD;
	else if (log_state == STATE_MISSING) {
	    if (sts == STS_OK)
		log_state = STATE_OK;
	    else
		log_state = STATE_BAD;
	}
    }

    return sts;
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "verbose", 0, 'v', 0, "verbose output" },
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "aD:dilLmst:vZ:z?",
    .long_options = longopts,
    .short_usage = "[options] archive [metricname ...]",
};

int
main(int argc, char *argv[])
{
    int			c;
    int			sts;
    int			i;
    int			n;
    char		*p;
    __pmContext		*ctxp;
    struct dirent	**namelist;
    char 		*tz = NULL;		/* for -Z timezone */
    int			zflag = 0;		/* for -z */
    int			nfile;

    sep = __pmPathSeparator();
    setlinebuf(stdout);
    setlinebuf(stderr);

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(opts.optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'v':	/* verbose */
	    vflag = 1;
	    break;

	case 'z':	/* timezone from host */
	    if (tz != NULL) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n", pmProgname);
		opts.errors++;
	    }
	    zflag++;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n", pmProgname);
		opts.errors++;
	    }
	    tz = opts.optarg;
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors || opts.optind > argc - 1) {
	pmUsageMessage(&opts);
	exit(1);
    }

    archpathname = argv[opts.optind];
    archbasename = strdup(basename(strdup(archpathname)));
    /*
     * treat foo, foo.index, foo.meta, foo.NNN as all equivalent
     * to "foo"
     */
    p = strrchr(archbasename, '.');
    if (p != NULL) {
	if (strcmp(p, ".index") == 0 || strcmp(p, ".meta") == 0)
	    *p = '\0';
	else {
	    char	*q = ++p;
	    if (isdigit(*q)) {
		q++;
		while (*q && isdigit(*q))
		    q++;
		if (*q == '\0')
		    *p = '\0';
	    }
	}
    }
    archdirname = dirname(strdup(archpathname));
    if (vflag)
	printf("Scanning for components of archive \"%s\"\n", archpathname);
    nfile = scandir(archdirname, &namelist, filter, NULL);
    if (nfile < 1) {
	fprintf(stderr, "%s: No PCP archive files match \"%s\"\n", pmProgname, archpathname);
	exit(1);
    }
    /*
     * Pass 0 for data and metadata files
     */
    sts = STS_OK;
    for (i = 0; i < nfile; i++) {
	char	path[MAXPATHLEN];
	snprintf(path, sizeof(path), "%s%c%s", archdirname, sep, namelist[i]->d_name);
	if (pass0(path) == STS_FATAL)
	    /* unrepairable or unrepaired error */
	    sts = STS_FATAL;
    }
    if (meta_state == STATE_MISSING) {
	fprintf(stderr, "%s: Missing metadata file (%s%c%s.meta)\n", pmProgname, archdirname, sep, archbasename);
	sts = STS_FATAL;
    }
    if (log_state == STATE_MISSING) {
	fprintf(stderr, "%s: Missing log file (%s%c%s.0 or similar)\n", pmProgname, archdirname, sep, archbasename);
	sts = STS_FATAL;
    }

    if (sts == STS_FATAL) {
	if (vflag) printf("Cannot continue ... bye\n");
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, archpathname)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n", pmProgname, archpathname, pmErrStr(sts));
	fprintf(stderr, "Checking abandoned.\n");
	exit(1);
    }
    if ((n = pmWhichContext()) >= 0) {
	if ((ctxp = __pmHandleToPtr(n)) == NULL) {
	    fprintf(stderr, "%s: Botch: __pmHandleToPtr(%d) returns NULL!\n", pmProgname, n);
	    exit(1);
	}
    }
    else {
	fprintf(stderr, "%s: Botch: %s!\n", pmProgname, pmErrStr(PM_ERR_NOCONTEXT));
	exit(1);
    }
    /*
     * Note: ctxp->c_lock remains locked throughout ... __pmHandleToPtr()
     *       is only called once, and a single context is used throughout
     *       ... so there is no PM_UNLOCK(ctxp->c_lock) anywhere in the
     *       pmchecklog code.
     *       This works because ctxp->c_lock is a recursive lock and
     *       pmchecklog is single-threaded.
     */

    pass1(ctxp);

    exit(0);
}
