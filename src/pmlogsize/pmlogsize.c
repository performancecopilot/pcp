/*
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

int		dflag;		/* detail off by default */
int		rflag;		/* replication off by default */
int		vflag;		/* verbose off by default */
int		thres = -1;	/* cut-off percentage from -x for -d */

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    { "detail", 0, 'd', 0, "detailed output (per metric and per indom)" },
    PMOPT_DEBUG,
    { "replication", 0, 'r', 0, "report replicated metric values and instances" },
    { "verbose", 0, 'v', 0, "verbose output" },
    { "threshold", 1, 'x', "thres", "cull detailed report after thres % of space reported" },
    PMOPT_HOSTZONE,
    PMOPT_TIMEZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_DONE | PM_OPTFLAG_STDOUT_TZ,
    .short_options = "dD:rvx:zZ:?",
    .long_options = longopts,
    .short_usage = "[options] archive",
};

static void
do_work(char *fname)
{
    __pmFILE		*f;
    __pmLogLabel	label;
    int			sts;
    int			len;
    long		extsize;
    long		intsize;
    struct stat		sbuf;

    if ((f = __pmFopen(fname, "r")) == NULL) {
	fprintf(stderr, "%s: cannot open file: %s\n", fname, osstrerror());
	return;
    }
    if ((sts = __pmFread(&len, 1, sizeof(len), f)) != sizeof(len)) {
	fprintf(stderr, "%s: label header read botch: read returns %d not %d as expected\n", fname, sts, (int)sizeof(int));
	exit(1);
    }
    if ((sts = __pmFread(&label, 1, sizeof(label), f)) != sizeof(label)) {
	fprintf(stderr, "%s: label read botch: read returns %d not %d as expected\n", fname, sts, (int)sizeof(label));
	exit(1);
    }
    if ((sts = __pmFread(&len, 1, sizeof(len), f)) != sizeof(len)) {
	fprintf(stderr, "%s: label trailer read botch: read returns %d not %d as expected\n", fname, sts, (int)sizeof(int));
	exit(1);
    }
    stat(fname, &sbuf);
    extsize = sbuf.st_size;
    __pmFstat(f, &sbuf);
    intsize = sbuf.st_size;
    printf("%s:", fname);
    if (intsize != extsize) {
	printf(" [compression reduces size below by about %.0f%%]", 100*(float)(intsize - extsize) / intsize);
    }
    putchar('\n');

    label.ill_vol = ntohl(label.ill_vol);

    if (label.ill_vol == PM_LOG_VOL_TI)
	do_index(f);
    else if (label.ill_vol == PM_LOG_VOL_META)
	do_meta(f);
    else
	do_data(f, fname);
}

int
main(int argc, char *argv[])
{
    int			c;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'd':	/* bump detail reporting */
	    dflag++;
	    break;

	case 'r':	/* bump replication reporting */
	    rflag++;
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
	if (access(argv[opts.optind], F_OK) == 0)
	    do_work(argv[opts.optind]);
	else {
	    /*
	     * may be the basename of an archive, so process all
	     * matching file names
	     */
	    fprintf(stderr, "TODO handle arg %s\n", argv[opts.optind]);
	}
	opts.optind++;
    }

    return 0;
}
