/*
 * Copyright (c) 2016-2017 Red Hat.
 *
 * Test helper program for exercising pmMergeLabels(3).
 */

#include <ctype.h>
#include <assert.h>
#include <pcp/pmapi.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		nsets;
    int		errflag = 0;
    char	**sets, result[PM_MAXLABELJSONLEN];
    static char	*usage = "labels [[labels] ...]";

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {
	case 'D':	/* debug flag */
	    if ((sts = pmSetDebug(optarg)) < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
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

    nsets = argc - optind;
    if (errflag || nsets < 1) {
	printf("Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    if ((sets = calloc(nsets, sizeof(char *))) == NULL) {
	perror("calloc");
	exit(1);
    }

    for (c = 0; c < nsets; c++)
	sets[c] = argv[optind++];

    /* iterate over remaining arguments (labels) and build merge set */
    if ((sts = pmMergeLabels(sets, nsets, result, sizeof(result))) < 0) {
	fprintf(stderr, "pmMergeLabels: %s\n", pmErrStr(sts));
	exit(1);
    }
    puts(result);
    exit(0);
}
