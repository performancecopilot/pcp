/*
 * Copyright (c) 2021 Red Hat.
 * Copyright (c) 2018 Ken McDonell.  All Rights Reserved.
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
#include "logsize.h"
#include <sys/stat.h>

int		dflag;		/* detail off by default */
int		rflag;		/* replication off by default */
int		vflag;		/* verbose off by default */
int		thres = -1;	/* cut-off percentage from -x for -d */

static char	*argbasename;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    { "detail", 0, 'd', 0, "detailed output (per metric and per indom)" },
    PMOPT_DEBUG,
    { "replication", 0, 'r', 0, "report replicated metric values and instances" },
    { "verbose", 0, 'v', 0, "verbose output" },
    { "threshold", 1, 'x', "thres", "cull detailed report after thres % of space reported" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_DONE | PM_OPTFLAG_STDOUT_TZ,
    .short_options = "dD:rvx:?",
    .long_options = longopts,
    .short_usage = "[options] archive",
};

static int
filter(
#ifdef HAVE_CONST_DIRENT
	const struct dirent *dp
#else
	struct dirent *dp
#endif
	)
{
    char logBase[MAXPATHLEN];
    static int	len = -1;

    if (dp == NULL) {
	/* reset */
	len = -1;
	return 0;
    }

    if (len == -1) {
	len = strlen(argbasename);
	if (vflag > 2) {
	    fprintf(stderr, "argbasename=\"%s\" len=%d\n", argbasename, len);
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
    if (strcmp(logBase, argbasename) != 0) {
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

static void
do_work(char *fname)
{
    __pmFILE		*f;
    int			sts;
    int			len;
    int			magic;
    int			volume;
    long		extsize;
    long		intsize;
    long		saved;
    struct stat		sbuf;

    if ((f = __pmFopen(fname, "r")) == NULL) {
	fprintf(stderr, "%s: cannot open file: %s\n", fname, osstrerror());
	return;
    }
    if ((sts = __pmFread(&len, 1, sizeof(len), f)) != sizeof(len)) {
	fprintf(stderr, "%s: label header read botch: read returns %d not %d as expected\n", fname, sts, (int)sizeof(int));
	exit(1);
    }
    len = ntohl(len);

    saved = __pmFtell(f);
    if ((sts = __pmFread(&magic, 1, sizeof(magic), f)) != sizeof(magic)) {
	fprintf(stderr, "%s: label magic read botch: read returns %d not %d as expected\n", fname, sts, (int)sizeof(magic));
	exit(1);
    }
    __pmFseek(f, saved, SEEK_SET);
    magic = ntohl(magic);

    if ((magic & 0xff) >= PM_LOG_VERS03) {
	__pmExtLabel_v3	label3;
	char		buffer[1<<16];

	if (len < sizeof(__pmExtLabel_v3) + 2*sizeof(int)) {
	    fprintf(stderr, "%s: label header botch: read %d not >=%d as expected\n", fname, len, (int)(sizeof(__pmExtLabel_v3) + 2*sizeof(int)));
	    exit(1);
	}
	if ((sts = __pmFread(&label3, 1, sizeof(label3), f)) != sizeof(label3)) {
	    fprintf(stderr, "%s: label read botch: read returns %d not %d as expected\n", fname, sts, (int)sizeof(label3));
	    exit(1);
	}
	volume = ntohl(label3.vol);

	len -= (sizeof(__pmExtLabel_v3) + 2*sizeof(int));
	if (len > sizeof(buffer)) {
	    fprintf(stderr, "%s: label strings botch: size %d not <=%d as expected\n", fname, len, (int)sizeof(buffer));
	    exit(1);
	}
	if ((sts = __pmFread(buffer, 1, len, f)) != len) {
	    fprintf(stderr, "%s: label strings botch: read returns %d not %d as expected\n", fname, sts, len);
	    exit(1);
	}

    } else {
	__pmExtLabel_v2	label2;

	if (len < sizeof(__pmExtLabel_v2) + 2*sizeof(int)) {
	    fprintf(stderr, "%s: label header size botch: read %d not >=%d as expected\n", fname, len, (int)(sizeof(__pmExtLabel_v2) + 2*sizeof(int)));
	    exit(1);
	}
	if ((sts = __pmFread(&label2, 1, sizeof(label2), f)) != sizeof(label2)) {
	    fprintf(stderr, "%s: label read botch: read returns %d not %d as expected\n", fname, sts, (int)sizeof(label2));
	    exit(1);
	}
	volume = ntohl(label2.vol);
    }

    if ((sts = __pmFread(&len, 1, sizeof(len), f)) != sizeof(len)) {
	fprintf(stderr, "%s: label trailer read botch: read returns %d not %d as expected\n", fname, sts, (int)sizeof(int));
	exit(1);
    }
    if ((sts = stat(fname, &sbuf)) != 0) {
	fprintf(stderr, "%s: cannot stat file: %s\n", fname, osstrerror());
	exit(1);
    }
    extsize = sbuf.st_size;
    if ((sts = __pmFstat(f, &sbuf)) != 0) {
	fprintf(stderr, "%s: cannot __pmFstat file: %s\n", fname, osstrerror());
	exit(1);
    }
    intsize = sbuf.st_size;
    printf("%s:", fname);
    if (intsize != extsize) {
	printf(" [compression reduces size below by about %.0f%%]", 100*(float)(intsize - extsize) / intsize);
    }
    putchar('\n');

    if (volume == PM_LOG_VOL_TI)
	do_index(f, magic & 0xff);
    else if (volume == PM_LOG_VOL_META)
	do_meta(f);
    else
	do_data(f, fname);

    __pmFclose(f);
}

int
main(int argc, char *argv[])
{
    int			c;
    struct stat		sbuf;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'd':	/* bump detail reporting */
	    dflag = 1;
	    break;

	case 'r':	/* bump replication reporting */
	    dflag = rflag = 1;
	    break;

	case 'x':	/* cut-off threshold % for detailed reporting */
	    thres = -1;
	    thres = atoi(opts.optarg);
	    if (thres < 1 || thres > 100) {
		fprintf(stderr, "Bad arg for -x (%s): should be between 1 and 100\n", opts.optarg);
		exit(1);
	    }
	    break;

	case 'v':	/* bump verbosity */
	    vflag++;
	    break;
	}
    }

    if (!opts.errors && opts.optind >= argc) {
	fprintf(stderr, "Error: no archive specified\n\n");
	opts.errors++;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    setlinebuf(stderr);

    __pmAddOptArchive(&opts, argv[opts.optind]);
    opts.flags &= ~PM_OPTFLAG_DONE;
    __pmEndOptions(&opts);

    while (opts.optind < argc) {
	if (access(argv[opts.optind], F_OK) == 0 &&
	    stat(argv[opts.optind], &sbuf) == 0 &&
	    (sbuf.st_mode & S_IFMT) == S_IFREG) {
	    do_work(argv[opts.optind]);
	}
	else {
	    /*
	     * may be the basename of an archive, so process all
	     * matching file names
	     */
	    char	*tmp1 = strdup(argv[opts.optind]);
	    char	*tmp2 = strdup(argv[opts.optind]);
	    char	*argdirname;
	    int		nfile;
	    struct dirent	**filelist;
	    int		i;
	    char	sep;
	    char	path[MAXPATHLEN];

	    sep = pmPathSeparator();

	    argbasename = basename(tmp1);
	    argdirname = dirname(tmp2);

	    filter(NULL);
	    nfile = scandir(argdirname, &filelist, filter, alphasort);
	    if (nfile < 0) {
		fprintf(stderr, "Error: scandir(\"%s\", ...) failed: %s\n",  argdirname, pmErrStr(-errno));
		exit(1);
	    }
	    else if (nfile == 0) {
		fprintf(stderr, "Error: no PCP archive files match \"%s\"\n", argv[opts.optind]);
		exit(1);
	    }
	    for (i = 0; i < nfile; i++) {
		if (strcmp(argdirname, ".") == 0) {
		    /* skip ./ prefix */
		    strncpy(path, filelist[i]->d_name, sizeof(path));
		}
		else {
		    pmsprintf(path, sizeof(path), "%s%c%s", argdirname, sep, filelist[i]->d_name);
		}
		do_work(path);
		free(filelist[i]);
	    }
	    free(tmp1);
	    free(tmp2);
	    free(filelist);
	}
	opts.optind++;
    }

    return 0;
}
