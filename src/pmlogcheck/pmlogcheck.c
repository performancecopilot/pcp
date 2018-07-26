/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
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

#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include "pmapi.h"
#include "libpcp.h"
#include "logcheck.h"

char		sep;
int		vflag;		/* verbose off by default */
int		nowrap;		/* suppress wrap check */
int		index_state = STATE_MISSING;
int		meta_state = STATE_MISSING;
int		log_state = STATE_MISSING;
int		mark_count;
int		result_count;
__pmLogLabel	log_label;

static char	*archbasename;	/* after basename() */

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "label", 0, 'l', 0, "print the archive label" },
    PMOPT_NAMESPACE,
    PMOPT_START,
    PMOPT_FINISH,
    { "verbose", 0, 'v', 0, "verbose output" },
    { "nowrap", 0, 'w', 0, "suppress counter wrap warnings" },
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_DONE | PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
    .short_options = "D:ln:S:T:zvwZ:?",
    .long_options = longopts,
    .short_usage = "[options] archive",
};

static void
dumpLabel(void)
{
    pmLogLabel	label;
    char	*ddmm;
    char	*yr;
    int		sts;
    char	timebuf[32];	/* for pmCtime result + .xxx */
    time_t	time;

    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: cannot get archive label record: %s\n",
		pmGetProgname(), pmErrStr(sts));
	exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Log Label (Log Format Version %d)\n", label.ll_magic & 0xff);
    fprintf(stderr, "Performance metrics from host %s\n", label.ll_hostname);

    time = label.ll_start.tv_sec;
    ddmm = pmCtime(&time, timebuf);
    ddmm[10] = '\0';
    yr = &ddmm[20];
    fprintf(stderr, "  commencing %s ", ddmm);
    pmPrintStamp(stderr, &label.ll_start);
    fprintf(stderr, " %4.4s\n", yr);

    if (opts.finish.tv_sec == INT_MAX) {
        /* pmGetArchiveEnd() failed! */
        fprintf(stderr, "  ending     UNKNOWN\n");
    }
    else {
	time = opts.finish.tv_sec;
        ddmm = pmCtime(&time, timebuf);
        ddmm[10] = '\0';
        yr = &ddmm[20];
        fprintf(stderr, "  ending     %s ", ddmm);
        pmPrintStamp(stderr, &opts.finish);
        fprintf(stderr, " %4.4s\n", yr);
    }
}

static int
filter(const_dirent *dp)
{
    char logBase[MAXPATHLEN];
    static int	len = -1;

    if (len == -1) {
	len = strlen(archbasename);
	if (vflag > 2) {
	    fprintf(stderr, "archbasename=\"%s\" len=%d\n", archbasename, len);
	}
    }
    if (vflag > 2)
	fprintf(stderr, "d_name=\"%s\"? ", dp->d_name);

    if (dp->d_name[len] != '.') {
	if (vflag > 2)
	    fprintf(stderr, "no (not expected extension after basename)\n");
	return 0;
    }
    /*
     * __pmLogBaseName will strip the suffix by modifying the data
     * in place. The suffix can still be found after the base name.
     */
    strncpy(logBase, dp->d_name, sizeof(logBase));
    logBase[sizeof(logBase)-1] = '\0';
    if (__pmLogBaseName(logBase) == NULL ) {
	if (vflag > 2)
	    fprintf(stderr, "no (not expected extension after basename)\n");
	return 0;
    }
    if (strcmp(logBase, archbasename) != 0) {
	if (vflag > 2)
	    fprintf(stderr, "no (first %d chars not matched)\n", len);
	return 0;
    }
    if (strcmp(&logBase[len+1], "meta") == 0) {
	if (vflag > 2)
	    fprintf(stderr, "yes\n");
	return 1;
    }
    if (strcmp(&logBase[len+1], "index") == 0) {
	if (vflag > 2)
	    fprintf(stderr, "yes\n");
	return 1;
    }
    if (! isdigit((int)(logBase[len+1]))) {
	if (vflag > 2)
	    fprintf(stderr, "no (non-digit after basename)\n");
	return 0;
    }
    if (vflag > 2)
	fprintf(stderr, "yes\n");
    return 1;
}

int
main(int argc, char *argv[])
{
    int			c;
    int			sts;
    int			ctx;
    int			i;
    int			lflag = 0;	/* no label by default */
    int			nfile;
    int			n;
    char		*p;
    struct dirent	**namelist;
    __pmContext		*ctxp;
    char		*archpathname;	/* from the command line */
    char		*archdirname;	/* after dirname() */
    char		archname[MAXPATHLEN];	/* full pathname to base of archive name */

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'l':	/* display the archive label */
	    lflag = 1;
	    break;
	case 'v':	/* bump verbosity */
	    vflag++;
	    break;
	case 'w':	/* no wrap checks */
	    nowrap = 1;
	    break;
	}
    }

    if (!opts.errors && opts.optind >= argc) {
	pmprintf("Error: no archive specified\n\n");
	opts.errors++;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    sep = pmPathSeparator();
    setlinebuf(stderr);

    __pmAddOptArchive(&opts, argv[opts.optind]);
    opts.flags &= ~PM_OPTFLAG_DONE;
    __pmEndOptions(&opts);

    archpathname = argv[opts.optind];
    archbasename = strdup(basename(strdup(archpathname)));
    /*
     * treat foo.index, foo.meta, foo.NNN along with any supported
     * compressed file suffixes as all equivalent
     * to "foo"
     */
    p = strrchr(archbasename, '.');
    if (p != NULL) {
	char	*q = p + 1;
	if (isdigit((int)*q)) {
	    /*
	     * foo.<digit> ... if archpathname does exist, then
	     * safe to strip digits, else leave as is for the
	     * case of, e.g. archive-20150415.041154 which is the
	     * pmmgr basename for an archive with a first volume
	     * named archive-20150415.041154.0
	     */
	    if (access(archpathname, F_OK) == 0)
		__pmLogBaseName(archbasename);
	}
	else
	    __pmLogBaseName(archbasename);
    }

    archdirname = dirname(strdup(archpathname));
    if (vflag)
	fprintf(stderr, "Scanning for components of archive \"%s\"\n", archpathname);
    nfile = scandir(archdirname, &namelist, filter, NULL);
    if (nfile < 1) {
	fprintf(stderr, "%s: no PCP archive files match \"%s\"\n", pmGetProgname(), archpathname);
	exit(EXIT_FAILURE);
    }

    /*
     * Pass 0 for data, metadata and index files ... check physical
     * archive record structure, then label record
     */
    sts = STS_OK;
    for (i = 0; i < nfile; i++) {
	char	path[MAXPATHLEN];
	if (strcmp(archdirname, ".") == 0) {
	    /* skip ./ prefix */
	    strncpy(path, namelist[i]->d_name, sizeof(path));
	}
	else {
	    pmsprintf(path, sizeof(path), "%s%c%s", archdirname, sep, namelist[i]->d_name);
	}
	if (pass0(path) == STS_FATAL)
	    /* unrepairable or unrepaired error */
	    sts = STS_FATAL;
    }
    if (meta_state == STATE_MISSING) {
	fprintf(stderr, "%s%c%s.meta: missing metadata file\n", archdirname, sep, archbasename);
	sts = STS_FATAL;
    }
    if (log_state == STATE_MISSING) {
	fprintf(stderr, "%s%c%s.0 (or similar): missing log file \n", archdirname, sep, archbasename);
	sts = STS_FATAL;
    }

    if (sts == STS_FATAL) {
	if (vflag) fprintf(stderr, "Due to earlier errors, cannot continue ... bye\n");
	exit(EXIT_FAILURE);
    }

    if ((sts = ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archpathname)) < 0) {
	fprintf(stderr, "%s: cannot open archive \"%s\": %s\n", pmGetProgname(), archpathname, pmErrStr(sts));
	fprintf(stderr, "Checking abandoned.\n");
	exit(EXIT_FAILURE);
    }

    if (pmGetContextOptions(ctx, &opts) < 0) {
        pmflush();      /* runtime errors only at this stage */
        exit(EXIT_FAILURE);
    }

    if (lflag)
	dumpLabel();

    if ((n = pmWhichContext()) >= 0) {
	if ((ctxp = __pmHandleToPtr(n)) == NULL) {
	    fprintf(stderr, "%s: botch: __pmHandleToPtr(%d) returns NULL!\n", pmGetProgname(), n);
	    exit(EXIT_FAILURE);
	}
    }
    else {
	fprintf(stderr, "%s: botch: %s!\n", pmGetProgname(), pmErrStr(PM_ERR_NOCONTEXT));
	exit(EXIT_FAILURE);
    }
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
    PM_UNLOCK(ctxp->c_lock);

    if (strcmp(archdirname, ".") == 0)
	/* skip ./ prefix */
	strncpy(archname, archbasename, sizeof(archname) - 1);
    else
	pmsprintf(archname, sizeof(archname), "%s%c%s", archdirname, sep, archbasename);

    sts = pass1(ctxp, archname);

    if (index_state == STATE_BAD) {
	/* prevent subsequent use of bad temporal index */
	ctxp->c_archctl->ac_log->l_numti = 0;
    }

    sts = pass2(ctxp, archname);

    sts = pass3(ctxp, archname, &opts);

    if (vflag) {
	if (result_count > 0)
	    fprintf(stderr, "Processed %d pmResult records\n", result_count);
	if (mark_count > 0)
	    fprintf(stderr, "Processed %d <mark> records\n", mark_count);
    }

    return 0;
}
