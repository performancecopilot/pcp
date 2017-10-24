
/*
 * Copyright (c) 2016 Red Hat Inc.  All Rights Reserved.
 *
 * Make pmSemStr jump through hoops
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		max, min;
    static char	*usage = "[-D debugspec]";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc) {
	printf("Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    printf("%d -> %s\n", PM_SEM_COUNTER, pmSemStr(PM_SEM_COUNTER));
    printf("%d -> %s\n", PM_SEM_INSTANT, pmSemStr(PM_SEM_INSTANT));
    printf("%d -> %s\n", PM_SEM_DISCRETE, pmSemStr(PM_SEM_DISCRETE));

    printf("\nAnd now some error cases ...\n");
    max = PM_SEM_COUNTER > PM_SEM_INSTANT ? PM_SEM_COUNTER : PM_SEM_INSTANT;
    if (PM_SEM_DISCRETE > max)
	max = PM_SEM_DISCRETE;

    min = PM_SEM_COUNTER < PM_SEM_INSTANT ? PM_SEM_COUNTER : PM_SEM_INSTANT;
    if (PM_SEM_DISCRETE < min)
	min = PM_SEM_DISCRETE;

    printf("out of range high -> %s\n", pmSemStr(max + 1));
    printf("out of range low -> %s\n", pmSemStr(min + 1));

    exit(0);
}
