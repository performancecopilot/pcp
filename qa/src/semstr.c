
/*
 * Copyright (c) 2016 Red Hat Inc.  All Rights Reserved.
 *
 * Make pmSemStr jump through hoops
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#define MAXIMUM(x, y)   ((x) > (y) ? (x) : (y))
#define MINIMUM(x, y)   ((x) < (y) ? (x) : (y))

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		max, min;
    static char	*usage = "[-D debugspec]";

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
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
	printf("Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    printf("%d -> %s\n", PM_SEM_COUNTER, pmSemStr(PM_SEM_COUNTER));
    printf("%d -> %s\n", PM_SEM_INSTANT, pmSemStr(PM_SEM_INSTANT));
    printf("%d -> %s\n", PM_SEM_DISCRETE, pmSemStr(PM_SEM_DISCRETE));

    printf("\nAnd now some error cases ...\n");

    max = MAXIMUM(PM_SEM_COUNTER, PM_SEM_INSTANT);
    max = MAXIMUM(max, PM_SEM_DISCRETE);
    printf("out of range high -> %s\n", pmSemStr(max + 1));

    min = MINIMUM(PM_SEM_COUNTER, PM_SEM_INSTANT);
    min = MINIMUM(min, PM_SEM_DISCRETE);
    printf("out of range low -> %s\n", pmSemStr(min + 1));

    exit(0);
}
