/*
 * Copyright (c) 2024 Red Hat.
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * exercise fetch with unusually large pmID count
 */

#include <pcp/pmapi.h>

#define NPMIDS	(128 * 1024)

int
main(int argc, char **argv)
{
    int		npmids = NPMIDS;
    pmID	pmid = pmID_build(2, 0, 7);	/* pmcd.version */
    pmID	*pmids;
    pmResult	*rp;
    int		sts;
    int		i;

    sts = pmNewContext(PM_CONTEXT_HOST, "local:");
    if (sts < 0) {
	fprintf(stderr, "pmNewContext(local:) failed: %s\n", pmErrStr(sts));
	exit(1);
    }

    if (argc > 1) npmids = atoi(argv[1]);

    if ((pmids = malloc(npmids * sizeof(pmID))) == NULL) {
	fprintf(stderr, "malloc failed: %s\n", pmErrStr(-ENOMEM));
	exit(1);
    }
    for (i = 0; i < npmids; i++)
	pmids[i] = pmid;

    sts = pmFetch(npmids, pmids, &rp);
    free(pmids);

    if (sts < 0)
	printf("expected an error -- got [%d] %s\n", sts, pmErrStr(sts));
    else {
	printf("expected error but was successful (%d returned)\n", sts);
	pmFreeResult(rp);
    }

    exit(0);
}
