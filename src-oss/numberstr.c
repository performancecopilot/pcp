/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <stdio.h>
#include <pcp/pmapi.h>

extern char *pmProgname;

int
main(int argc, char *argv[])
{
    double	d;
    char	*endptr;

    pmProgname = basename(argv[0]);
    if (argc != 2) {
	fprintf(stderr, "Usage: %s double\n", pmProgname);
	exit(1);
    }

    d = strtod(argv[1], &endptr);
    if (endptr != NULL && endptr[0] != '\0') {
	fprintf(stderr, "%s does not smell like a double, bozo!\n", argv[1]);
	exit(1);
    }

    printf("%s\n", pmNumberStr(d));

    return 0;
}
