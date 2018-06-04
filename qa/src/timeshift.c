/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2018 Ken McDonell.  All Rights Reserved.
 *
 * Compute timeshift (pmlogrewrite format) to move start of archive
 * to a new ctime.
 *
 * Usage: timeshift [-z] -a archive "ctime"
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static pmLongOptions longopts[] = {
    PMOPT_ARCHIVE,
    PMOPT_HOSTZONE,
    PMAPI_OPTIONS_HEADER("timeshift options"),
    { "verbose", 0, 'v', "N", "diagnostic output" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "a:vz?",
    .long_options = longopts,
    .short_usage = "[options] \"ctime\"",
};

int
main(int argc, char **argv)
{
    int			c;
    int			sts;
    int			vflag = 0;
    int			ctx;
    __pmContext		*ctxp;
    time_t		t;
    struct tm		old;
    struct tm		new;
    struct timeval	epoch = { 0, 0 };
    struct timeval	tv;
    char		*errmsg;
    int			delta;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'v':	/* verbose diagnostics */
	    vflag++;
	    break;
	}
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.narchives == 1) {
	if ((ctx = pmNewContext(PM_CONTEXT_ARCHIVE, opts.archives[0])) < 0) {
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), opts.archives[0],  pmErrStr(ctx));
	    exit(EXIT_FAILURE);
	}
	if ((sts = pmGetContextOptions(ctx, &opts)) < 0) {
	    pmflush();
	    fprintf(stderr, "%s: pmGetContextOptions(%d, ...) failed: %s\n",
			pmGetProgname(), pmWhichContext(), pmErrStr(sts));
		exit(EXIT_FAILURE);
	}
    }
    else {
	fprintf(stderr, "%s: exactly one archive argument required\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }

    if (opts.optind != argc-1)
	opts.errors++;

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if ((ctxp = __pmHandleToPtr(ctx)) == NULL) {
	fprintf(stderr, "__pmHandleToPtr failed: eh?\n");
	exit(EXIT_FAILURE);
    }

    t = ctxp->c_archctl->ac_log->l_label.ill_start.tv_sec;

    pmLocaltime(&t, &old);

    if (vflag)
	fprintf(stderr, "label: %ld %04d-%02d-%02d %02d:%02d:%02d\n", 
	    (long)t, old.tm_year+1900, old.tm_mon+1, old.tm_mday, old.tm_hour, old.tm_min, old.tm_sec);

    if (__pmParseCtime(argv[argc-1], &new, &errmsg) != 0) {
	fprintf(stderr, "__pmParseCtime: failed:\n%s\n", errmsg);
	exit(EXIT_FAILURE);
    }
    __pmConvertTime(&new, &epoch, &tv);

    if (vflag)
	fprintf(stderr, "want: %ld %04d-%02d-%02d %02d:%02d:%02d\n", 
	    (long)tv.tv_sec, new.tm_year+1900, new.tm_mon+1, new.tm_mday, new.tm_hour, new.tm_min, new.tm_sec);

    delta = tv.tv_sec - t;

    if (delta < 0) {
	printf("-");
	delta = -delta;
    }
    printf("%d:%02d:%02d\n", delta / 3600, (delta % 3600) / 60, (delta % 3600) % 60);

    return 0;
}
