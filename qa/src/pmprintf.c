/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pcp/pmapi.h>

int
main(int argc, char *argv[])
{
    int		sts, i;
    int		fd;

    if (argc < 2) {
	fprintf(stderr, "Bad pmprintf invocation (need more args)\n");
	exit(1);
    }

    /*
     * Some stdio environments start with strange fd's open ... close
     * 'em all to give us some breathing space
     */
    for (fd = 3; fd < 100; fd++)
	close(fd);

    /*
     * for testing #634665 it helps to have a couple of extra
     * open files ... see qa/505
     * ProPack 2.2 increased this to need 5 extra ones
     */
    (void)open("/dev/null", O_RDONLY);
    (void)open("/dev/null", O_RDONLY);
    (void)open("/dev/null", O_RDONLY);
    (void)open("/dev/null", O_RDONLY);
    (void)open("/dev/null", O_RDONLY);
    (void)open("/dev/null", O_RDONLY);
    (void)open("/dev/null", O_RDONLY);

    for (i = 1; i < argc; i++) {
	if ((sts = pmprintf("%s ", argv[i])) < 0) {
	    fprintf(stderr, "pmprintf: argv[%d]: \"%s\": %s\n", i, argv[i],  pmErrStr(sts));
	    break;
	}
    }

    pmprintf("\n");
    pmflush();
    exit(0);
}
