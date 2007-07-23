/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ident "$Id: xxx.c,v 1.1 2002/10/21 00:59:56 kenmcd Exp $"

#include <unistd.h>
#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main()
{
    int		i;
    char	*namelist[20];
    pmID	pmidlist[20];
    int		n;
    int		numpmid;
    pmResult	*resp;
    void	*mem;

    extern int pmDebug;
    pmDebug = DBG_TRACE_PDU;
    pmDebug = 0;

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
    namelist[i++] = "sample.sysinfo";
    namelist[i++] = "sampledso.seconds";
    namelist[i++] = "sample.colour";
    namelist[i++] = "sampledso.mirage";
    namelist[i++] = "sample.lights";
    namelist[i++] = "sampledso.string.hullo";
    namelist[i++] = "sample.aggregate.hullo";
    namelist[i++] = "sampledso.longlong.one";
    numpmid = i;
    n = pmLookupName(numpmid, namelist, pmidlist);
    if (n < 0) {
	fprintf(stderr, "pmLookupName: %s\n", pmErrStr(n));
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		printf("	%s - not known\n", namelist[i]);
	}
	exit(1);
    }

    mem = sbrk(0);
    for (i = 0; i < 10000; i++) {
	if ((n = pmFetch(numpmid, pmidlist, &resp)) < 0) {
	    fprintf(stderr, "pmFetch: %s\n", pmErrStr(n));
	    abort();
	}
	pmFreeResult(resp);
    }
    printf("mem growth: %d Kbytes\n", ((int)sbrk(0) - (int)mem)/1024);

    exit(0);
    /*NOTREACHED*/
}
