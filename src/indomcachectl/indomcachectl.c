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
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

static int
allmyoptions(int opt, pmOptions *optsp)
{
    /* they all belong to me */
    return 1;
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    { "group", 1, 'g', "GROUP", "create cache file owned by group GROUP" },
    { "list", 0, 'l', NULL, "list indom cache contents" },
    { "mode", 1, 'm', "MODE", "create cache file with mode MODE (octal)" },
    { "user", 1, 'u', "USER", "create cache file owned user USER" },
    { "warning", 0, 'w', NULL, "issue warnings" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "g:lm:u:w?",
    .long_options = longopts,
    .short_usage = "[options] domain.serial",
    .override = allmyoptions,
};

int
main(int argc, char **argv)
{
    int		lflag = 0;
    int		wflag = 0;
    int		c;
    int		domain, serial;
    int		sts;
    int		badarg = 0;
    pmInDom	indom;
    char	*user = NULL;
    uid_t	uid = geteuid();
    struct passwd	*pwp = NULL;
    char	*group = NULL;
    gid_t	gid = getegid();
    struct group	*grp = NULL;
    int		mflag = 0;
    mode_t	mode = 0660;	/* default user:rw group:rw other:none */
    char	*endp;
    long	tmp_mode;
    int		sep = pmPathSeparator();
    char	pathname[MAXPATHLEN];

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	    case 'g':		/* group */
		grp = getgrnam(opts.optarg);
		if (grp == NULL) {
		    fprintf(stderr, "%s: Error: getgrnam(%s) failed\n", pmGetProgname(), opts.optarg);
		    badarg++;
		}
		else {
		    group = opts.optarg;
		    gid = grp->gr_gid;
		}
		break;

	    case 'l':		/* list/dump */
		lflag++;
		break;

	    case 'm':		/* mode */
		tmp_mode = strtol(opts.optarg, &endp, 8);
		if (*endp != '\0' || tmp_mode > 0777) {
		    fprintf(stderr, "%s: Error: mode (%s) is not a valid\n", pmGetProgname(), opts.optarg);
		    badarg++;
		}
		else {
		    mflag++;
		    mode = (mode_t)tmp_mode;
		}
		break;

	    case 'u':		/* user */
		pwp = getpwnam(opts.optarg);
		if (pwp == NULL) {
		    fprintf(stderr, "%s: Error: getpwnam(%s) failed\n", pmGetProgname(), opts.optarg);
		    badarg++;
		}
		else {
		    user = opts.optarg;
		    uid = pwp->pw_uid;
		}
		break;

	    case 'w':		/* warnings */
		wflag++;
		break;
	}

    }

    if (badarg)
	exit(1);

    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT) || opts.optind != argc-1) {
	int		exitsts;
	exitsts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(exitsts);
    }

    if ((user != NULL || group != NULL || mflag != 0) && lflag) {
	fprintf(stderr, "%s: Error: -l is mutually exclusive with -u, -g and -m\n", pmGetProgname());
	exit(1);
    }

    if (user != NULL && group == NULL) {
	/* -u and no -g, use user's default group */
	gid = pwp->pw_gid;
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
	int	inst;
	char	*iname;
	int	numinst = 0;
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: %s: failed to load indom cache file\n", pmGetProgname(), pmInDomStr(indom));
	    exit(1);
	}
	if ((sts = pmdaCacheOp(indom, PMDA_CACHE_ACTIVE)) < 0) {
	    if (wflag)
		fprintf(stderr, "Warning: %s: PMDA_CACHE_ACTIVE: %s\n", pmInDomStr(indom), pmErrStr(sts));
	}
	if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND)) < 0) {
	    if (wflag)
		fprintf(stderr, "Warning: %s: PMDA_CACHE_WALK_REWIND: %s\n", pmInDomStr(indom), pmErrStr(sts));
	}
	printf("Instance domain %s cache contents ...\n", pmInDomStr(indom));
	while ((inst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) != -1) {
	    if ((sts = pmdaCacheLookup(indom, inst, &iname, NULL)) < 0) {
		if (wflag) {
		    fprintf(stderr, "Warning: %s: Lookup failed for inst=%d: %s\n", pmInDomStr(indom), inst, pmErrStr(sts));
		}
		continue;
	    }
	    if (numinst == 0)
		printf("%8.8s %s\n", "inst_id", "inst_name");
	    printf("%8d %s\n", inst, iname);
	    numinst++;
	}
	if (numinst == 0)
	    printf("No entries\n");

	exit(0);
    }

    /*
     * initialize ...
     */
    pmsprintf(pathname, sizeof(pathname), "%s%cconfig%cpmda%c%s",
	pmGetOptionalConfig("PCP_VAR_DIR"), sep, sep, sep, pmInDomStr(indom));
    if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WRITE)) < 0) {
	fprintf(stderr, "Error: %s: create %s failed: %s\n", pmGetProgname(), pathname, pmErrStr(sts));
	exit(1);
    }
    if (user || group) {
	/* change owner and group */
	if (chown(pathname, uid, gid) < 0) {
	    fprintf(stderr, "%s: Error: created %s, but cannot change ownership to uid=%d gid=%d: %s\n", pmGetProgname(), pathname, uid, gid, pmErrStr(-oserror()));
	    exit(1);
	}
    }
    /* explicitly set mode ... */
    if (chmod(pathname, mode) < 0) {
	fprintf(stderr, "%s: Error: created %s, but cannot change mode to 0%o: %s\n", pmGetProgname(), pathname, mode, pmErrStr(-oserror()));
	exit(1);
    }

    exit(0);
}
