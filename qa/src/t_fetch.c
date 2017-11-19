/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/* Timeout test for FETCH PDUs in text mode.
*/

#include <pcp/pmapi.h>

#define StuffDom(metric, newdom) pmID_build(newdom, pmID_cluster(metric), pmID_item(metric))

#define NMETRICS 3

int
main(int argc, char *argv[])
{
    pmID	*ids;
    pmResult	*res;
    int		i, j;
    int		nm;
    int		s;
    long	dom;
    char	*end;

    pmSetProgname(argv[0]);

    if (argc < 2) {
	fprintf(stderr, "Usage: %s domain ...\n", pmGetProgname());
	exit(1);
    }
    if ((ids = (pmID *)malloc((argc - 1) * NMETRICS * sizeof(pmID))) == (pmID *)0) {
	perror("pmID malloc");
	exit(1);
    }
    nm = 0;
    for (i = 1; i < argc; i++) {
	dom = strtol(argv[i], &end, 10);
	if (*end != '\0') {
	    fprintf(stderr, "'%s' is not a numeric domain\n", argv[1]);
	    exit(1);
	}
	for (j = 0; j < NMETRICS; j++) {
	    ids[nm] = 1234;
	    StuffDom(ids[nm], dom);
	    nm++;
	}

    }

    if ((s = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "pmNewContext(..., \"localhost\"): %s\n",
		pmErrStr(s));
	exit(1);
    }

    if ((s = pmFetch(nm, ids, &res)) < 0) {
	fprintf(stderr, "pmFetch: %s\n", pmErrStr(s));
	exit(1);
    }
    else {
	for (i = 0; i < nm; i++) {
	    if (res->vset[0]->numval == 0)
		fprintf(stderr, "pmid[%d]: No value(s) returned!\n", i);
	    else if (res->vset[0]->numval < 0)
		fprintf(stderr, "pmid[%d]: %s\n", i, pmErrStr(res->vset[0]->numval));
	}
    }

    exit(0);
}
