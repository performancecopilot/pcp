/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * pmcd pmda was botched
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main()
{
    pmID	pmid;
    __pmID_int	*p = (__pmID_int *)&pmid;
    pmResult	*rp;
    int		sts;

    pmid = 0;
    p->domain = 2;		/* pmcd */
    p->cluster = 13;		/* bogus */

    sts = pmNewContext(PM_CONTEXT_HOST, "localhost");
    if (sts < 0) {
	fprintf(stderr, "pmNewContext(localhost) failed: %s\n", pmErrStr(sts));
	exit(1);
    }

    sts = pmFetch(1, &pmid, &rp);
    if (sts != 0)
	printf("expect no error, got: %d %s\n", sts, pmErrStr(sts));
    if (rp->numpmid != 1)
	printf("expect 1 pmid, got %d\n", rp->numpmid);
    if (rp->vset[0]->pmid != pmid) {
	printf("pmid mismatch! %s", pmIDStr(pmid));
	printf(" != %s\n", pmIDStr(rp->vset[0]->pmid));
    }
    if (rp->vset[0]->numval == 1)
	printf("%d values is bogus\n", rp->vset[0]->numval);
    else
	printf("no value in pmResult -- expected\n");

    exit(0);
}
