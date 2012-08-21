/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <time.h>

int
main(int argc, char *argv[])
{
    time_t	clock;
    char	*p;
    int		i;
    char	buf[28];

    i = pmLoadNameSpace(PM_NS_DEFAULT);
    if (i < 0) {
	printf("pmLoadNameSpace: %s\n", pmErrStr(i));
	exit(1);
    }

    for (--argc; argc > 0; argc--, argv++) {
	printf("Trying %s ...\n", argv[1]);
	i = pmNewContext(PM_CONTEXT_HOST, argv[1]);
	if (i < 0) {
	    printf("pmNewContext: %s\n", pmErrStr(i));
	    goto more;
	}
	i = pmNewContextZone();
	if (i < 0) {
	    printf("pmNewContextZone: %s\n", pmErrStr(i));
	    goto more;
	}

	i = pmWhichZone(&p);
	time(&clock);
	printf("zone: %d TZ: %s ctime: %s\n", i, p, pmCtime(&clock, buf));
more:	continue;
    }

    exit(0);
}
