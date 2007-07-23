/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ident "$Id: chkhelp.c,v 1.1 2002/10/21 00:59:56 kenmcd Exp $"

/*
 * exercise the help facilities
 */

#include <unistd.h>
#include <ctype.h>
#include <pcp/pmapi.h>

int
main()
{
    int		i;
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
    numpmid = i;
    n = pmLookupName(numpmid, namelist, pmidlist);
    if (n < 0) {
	fprintf(stderr, "pmLookupName: %s\n", pmErrStr(n));
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		fprintf(stderr, "	%s - not known\n", namelist[i]);
	}
	exit(1);
    }
    if ((n = pmLookupDesc(pmidlist[0], &desc)) < 0) {
	fprintf(stderr, "initial pmLookupDesc failed: %s\n", pmErrStr(n));
	exit(1);
    }

    for (i = 0; i < 2; i++) {
	if (i == 0)
	    fprintf(stderr, "\nDefault Context\n");
	else {
	    fprintf(stderr, "\nNew Context\n");
	    if ((n = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
		fprintf(stderr, "pmNewContext: %s\n", pmErrStr(n));
		exit(1);
	    }
	}

	if ((n = pmLookupText(pmidlist[0], PM_TEXT_ONELINE, &buf)) < 0)
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

	if ((n = pmLookupText(pmidlist[0], PM_TEXT_HELP, &buf)) < 0)
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

    exit(0);
    /*NOTREACHED*/
}
