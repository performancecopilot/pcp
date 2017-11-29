/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * exercise the help facilities
 */

#include <unistd.h>
#include <ctype.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

int
main()
{
    int		i;
    int		j;
    char	*namelist[2];
    pmID	pmidlist[2];
    int		n;
    int		numpmid;
    char	*buf;
    pmDesc	desc;

    if ((n = pmLoadNameSpace(PM_NS_DEFAULT)) < 0) {
	fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(n));
	exit(1);
    }

    if ((n = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "pmNewContext(..., \"localhost\"): %s\n",
		pmErrStr(n));
	exit(1);
    }

    i = 0;
    namelist[i++] = "sample.colour";
    namelist[i++] = "sample.byte_ctr";
    numpmid = i;
    n = pmLookupName(numpmid, namelist, pmidlist);
    if (n != numpmid) {
	fprintf(stderr, "pmLookupName: %s\n", pmErrStr(n));
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		fprintf(stderr, "	%s - not known\n", namelist[i]);
	}
	exit(1);
    }

    for (j = 0; j < 3; j++) {
	/* default context (j==0) then NewContext(j==1) then DupContext(j==2) */
	for (i = 0; i < numpmid; i++) {
	    if (j == 0)
		fprintf(stderr, "\nDefault Context\n");
	    else if (j == 1) {
		fprintf(stderr, "\nNew Context\n");
		if ((n = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
		    fprintf(stderr, "pmNewContext: %s\n", pmErrStr(n));
		    exit(1);
		}
	    }
	    else {
		fprintf(stderr, "\nDup Context\n");
		if ((n = pmDupContext()) < 0) {
		    fprintf(stderr, "pmDupContext: %s\n", pmErrStr(n));
		    exit(1);
		}
	    }

	    fprintf(stderr, "metric: %s\n", namelist[i]);
	    if ((n = pmLookupDesc(pmidlist[i], &desc)) < 0) {
		fprintf(stderr, "pmLookupDesc failed: %s\n", pmErrStr(n));
		exit(1);
	    }

	    if ((n = pmLookupText(pmidlist[i], PM_TEXT_ONELINE, &buf)) < 0)
		fprintf(stderr, "pmLookupText: %s\n", pmErrStr(n));
	    else {
		fprintf(stderr, "\nOneline Text: %s\n", buf);
		free(buf);
	    }

	    if ((n = pmLookupInDomText(desc.indom, PM_TEXT_ONELINE, &buf)) < 0)
		fprintf(stderr, "pmLookupInDomText: %s\n", pmErrStr(n));
	    else {
		fprintf(stderr, "\nOneline InDomText: %s\n", buf);
		free(buf);
	    }

	    if ((n = pmLookupText(pmidlist[i], PM_TEXT_HELP, &buf)) < 0)
		fprintf(stderr, "pmLookupText: %s\n", pmErrStr(n));
	    else {
		fprintf(stderr, "\nHelp Text: %s\n", buf);
		free(buf);
	    }

	    if ((n = pmLookupInDomText(desc.indom, PM_TEXT_HELP, &buf)) < 0)
		fprintf(stderr, "pmLookupInDomText: %s\n", pmErrStr(n));
	    else {
		fprintf(stderr, "\nHelp InDomText: %s\n", buf);
		free(buf);
	    }
	}
    }

    exit(0);
}
