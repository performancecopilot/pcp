/*
 * Copyright (c) 2017 Red Hat, Inc.  All Rights Reserved.
 *
 * Exercise context error handling bug, see qa/1096
 */
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char *argv[])
{
    int         c;
    char        *archive = "badarchives/empty";

    if ((c = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0) {
    	printf("pmNewContext(PM_CONTEXT_ARCHIVE, \"%s\")"
		"-> %d (expected)\n", archive, c);
	if ((c = pmNewContext(PM_CONTEXT_HOST, "localhost")) >= 0)
	    pmDestroyContext(c); /* SEGFAULT here without the fix */
    }

    printf("SUCCESS\n");
    exit(0);
}

