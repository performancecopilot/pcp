/*
 * Copyright (c) 2020 Red Hat.
 *
 * Test helper program for exercising the internal __pmAddLabels routine.
 */

#include <ctype.h>
#include <assert.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

#define LABELS_1	"{\"a\":true}"
#define LABELS_2	"{\"b\":0,\"c\":null}"
#define LABELS_3	"{\"d\":{\"e\":1}}"
#define LABELS_4	"{\"f\":{\"gee\":22,\"h\":333}}"

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    pmLabelSet	*set = NULL;
    const char	*sets[] = { LABELS_1, LABELS_2, LABELS_3, LABELS_4 };
    int		flags[] = { 0, PM_LABEL_OPTIONAL, PM_LABEL_COMPOUND,PM_LABEL_COMPOUND };

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

    if (errflag) {
	printf("Usage: %s\n", pmGetProgname());
	exit(1);
    }

    for (c = 0; c < sizeof(sets)/sizeof(char *); c++) {
	printf("Adding set %d:  %s\n", c, sets[c]);
	if ((sts = __pmAddLabels(&set, sets[c], flags[c] | PM_LABEL_ITEM)) < 0) {
	    fprintf(stderr, "Error parsing labels from set %d: %s\n", c,
			    pmErrStr(sts));
	    errflag++;
	}
	__pmDumpLabelSets(stdout, set, 1);
    }
    pmFreeLabelSets(set, 1);

    return errflag;
}
