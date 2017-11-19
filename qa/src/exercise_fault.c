/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * Exercise fault injection infrastructure.
 */

#include <pcp/pmapi.h>
#include <pcp/fault.h>
#include <string.h>
#include <errno.h>

static void
exercise(void)
{
    int		i;
    void	*p;
    for (i = 1; i <= 10; i++) {
	__pmFaultInject("QA:1", PM_FAULT_ALLOC);
	p = malloc(10);
	if (p == NULL)
	    fprintf(stderr, "malloc:1[%d] %s\n", i, strerror(errno));
	else
	    free(p);
	__pmFaultInject("QA:2", PM_FAULT_ALLOC);
	p = malloc(100);
	if (p == NULL)
	    fprintf(stderr, "malloc:2[%d] %s\n", i, strerror(errno));
	else
	    free(p);
	__pmFaultInject("QA:3", PM_FAULT_ALLOC);
	p = malloc(1000);
	if (p == NULL)
	    fprintf(stderr, "malloc:3[%d] %s\n", i, strerror(errno));
	else
	    free(p);
    }
}

int
main(int argc, char *argv[])
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*usage = "[-D debug]";

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
	fprintf(stderr, "Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    exercise();

    return 0;
}
