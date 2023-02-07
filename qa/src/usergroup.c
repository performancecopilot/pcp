/*
 * Exercise the __pmAcc*() functions to manipulate user and group
 * names and access controls.
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2023 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>

#define USERS	0
#define GROUPS	1

static int	mode = USERS;

/* from pmcd.h ... */
#define PMCD_OP_FETCH 0x1
#define PMCD_OP_FOO 0x2
#define PMCD_OP_STORE 0x4

static int overrides(int, pmOptions *);
static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_DEBUG,	/* -D */
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_HEADER("usergroup options"),
    { "group", 0, 'g', NULL, "args are group ids" },
    { "user", 0, 'u', NULL, "args are user ids [default]" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:gu",
    .long_options = longopts,
    .override = overrides,
};

static int
overrides(int opt, pmOptions *optsp)
{
    if (opt == 'g')
	mode = GROUPS;
    return 0;
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'u':	/* user ids */
		mode = USERS;
		break;

	case 'g':	/* group ids */
		/*
		 * really handled in overrides(), not here because -g
		 * is already assigned to PMOPT_GUIMODE for
		 * pmGetOptions()
		 */
		mode = GROUPS;
		break;
	}
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    sts = __pmAccAddOp(PMCD_OP_FETCH);
    if (sts < 0) {
	fprintf(stderr, "Arrgh, __pmAccAddOp(..FETCH) failed: %s\n", pmErrStr(sts));
	return(1);
    }
    sts = __pmAccAddOp(PMCD_OP_FOO);
    if (sts < 0) {
	fprintf(stderr, "Arrgh, __pmAccAddOp(..FOO) failed: %s\n", pmErrStr(sts));
	return(1);
    }
    sts = __pmAccAddOp(PMCD_OP_STORE);
    if (sts < 0) {
	fprintf(stderr, "Arrgh, __pmAccAddOp(..STORE) failed: %s\n", pmErrStr(sts));
	return(1);
    }

    while (opts.optind < argc) {
	if (mode == USERS) {
	    sts = __pmAccAddUser(argv[opts.optind], PMCD_OP_FETCH|PMCD_OP_STORE, PMCD_OP_STORE, 1);
	    if (sts < 0)
		fprintf(stderr, "__pmAccAddUser(%s, ...): %s\n", argv[opts.optind], pmErrStr(sts));
	}
	else {
	    sts = __pmAccAddGroup(argv[opts.optind], PMCD_OP_FETCH|PMCD_OP_FOO|PMCD_OP_STORE, PMCD_OP_STORE, 1);
	    if (sts < 0)
		fprintf(stderr, "__pmAccAddGroup(%s, ...): %s\n", argv[opts.optind], pmErrStr(sts));
	}
	opts.optind++;
    }

    if (mode == USERS)
	__pmAccDumpUsers(stderr);
    else
	__pmAccDumpGroups(stderr);

    return 0;
}
