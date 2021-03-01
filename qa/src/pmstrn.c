/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2021 Ken McDonell.  All Rights Reserved.
 *
 * Simple wrapper for pmstrncpy() and pmstrncat() exerciser
 */

#include <pcp/pmapi.h>

int
main(int argc, char **argv)
{
    char	dest[100];
    size_t	destlen;
    int		sts;
    int		i;

    pmSetProgname(argv[0]);

    if (argc < 3) {
	fprintf(stderr, "Usage: %s destlen str [str ...]\n", pmGetProgname());
	exit(1);
    }

    destlen = atoi(argv[1]);
    if (destlen > 100) {
	fprintf(stderr, "Error: destlen must be <= 100\n");
	exit(1);
    }

    dest[99] = '\01';

    sts = pmstrncpy(dest, destlen, argv[2]);
    printf("pmstrncpy(..., %d, \"%s\") -> %d dest=\"%s\"\n", (int)destlen, argv[2], sts, dest);
    if (dest[99] != '\01')
	printf("Warning: dest[99] over-written (%c)\n", dest[99]);

    for (i = 3; i < argc; i++) {
	printf("pmstrncat(\"%s\", %d, \"%s\") -> ", dest, (int)destlen, argv[i]);
	sts = pmstrncat(dest, destlen, argv[i]);
	printf("%d dest=\"%s\"\n", sts, dest);
	if (dest[99] != '\01')
	    printf("Warning: dest[99] over-written (%c)\n", dest[99]);
    }

    exit(0);
}
