/*
 * Copyright (c) 2024 Ken McDonell.  All Rights Reserved.
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
 *
 * Debug flags
 * <none>
 */

#include "pmapi.h"
#include "pmda.h"
#include <sys/types.h>
#include <pwd.h>

static pmLongOptions longopts[] = {
    { "list", 0, 'l', NULL, "list indom cache contents" },
    { "user", 1, 'u', "USER", "create cache file owned by this user" },
    { "warning", 0, 'w', NULL, "issue warnings" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "lu:w?",
    .long_options = longopts,
    .short_usage = "[options] domain.serial",
};

int
main(int argc, char **argv)
{
    int		lflag = 0;
    int		wflag = 0;
    int		c;
    int		domain, serial;
    int		sts;
    pmInDom	indom;
    char	*user = NULL;
    struct passwd	*pw;
    int		sep = pmPathSeparator();
    char	pathname[MAXPATHLEN];

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	    case 'l':		/* list/dump */
		lflag++;
		break;

	    case 'u':		/* user */
		user = opts.optarg;
		break;

	    case 'w':		/* warnings */
		wflag++;
		break;
	}

    }
    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT) || opts.optind != argc-1) {
	int		exitsts;
	exitsts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(exitsts);
    }

    if (user != NULL && lflag) {
	fprintf(stderr, "%s: Error: -l and -u options are mutually exclusive\n", pmGetProgname());
	exit(1);
    }

    if (user != NULL) {
	pw = getpwnam(user);
	if (pw == NULL) {
	    fprintf(stderr, "%s: Error: getpwnam(%s) failed\n", pmGetProgname(), user);
	    exit(1);
	}
    }

    if (sscanf(argv[opts.optind], "%u.%u", &domain, &serial) != 2) {
	fprintf(stderr, "%s: Error: bad format domain.serial: %s\n", pmGetProgname(), argv[opts.optind]);
	exit(1);
    }
    /*
     * domain is limited to 9 bits, serial to 22 bits ... both need
     * to be positive
     */
    if (domain < 0 || domain >= (1<<9) || serial < 0 || serial >= (1<<22)) {
	fprintf(stderr, "%s: Error: out-of-range domain.serial: %s\n", pmGetProgname(), argv[opts.optind]);
	exit(1);
    }
    indom = pmInDom_build(domain, serial);

    /*
     * try loading the cache ... needed for -l, doesn't matter otherwise
     */
    if ((sts = pmdaCacheOp(indom, PMDA_CACHE_LOAD)) < 0) {
	if (wflag)
	    fprintf(stderr, "Warning: %s: PMDA_CACHE_LOAD: %s\n", pmInDomStr(indom), pmErrStr(sts));
    }
    else if (!lflag) {
	/* don't clobber an existing indom cache file */
	fprintf(stderr, "%s: Error: %s: indom cache file already exists\n", pmGetProgname(), pmInDomStr(indom));
	exit(1);
    }

    if (lflag) {
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: %s: failed to load indom cache file\n", pmGetProgname(), pmInDomStr(indom));
	    exit(1);
	}
	if ((sts = pmdaCacheOp(indom, PMDA_CACHE_DUMP)) < 0) {
	    if (wflag)
		fprintf(stderr, "Warning: %s: PMDA_CACHE_DUMP: %s\n", pmInDomStr(indom), pmErrStr(sts));
	}
	exit(0);
    }

    /*
     * initialize ...
     */
    if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WRITE)) < 0) {
	if (wflag)
	    fprintf(stderr, "Warning: %s: PMDA_CACHE_WRITE: %s\n", pmInDomStr(indom), pmErrStr(sts));
    }
    if (user) {
	pmsprintf(pathname, sizeof(pathname), "%s%cconfig%cpmda%c%s",
	    pmGetOptionalConfig("PCP_VAR_DIR"), sep, sep, sep, pmInDomStr(indom));
	if (chown(pathname, pw->pw_uid, pw->pw_gid) < 0) {
	    fprintf(stderr, "%s: Error: created %s, but cannot change ownership: %s\n", pmGetProgname(), pathname, pmErrStr(-oserror()));
	    exit(1);
	}
    }

    exit(0);
}
