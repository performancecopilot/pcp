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
    char	dest[101];
    size_t	destlen;
    int		sts;
    int		i;
    int		a;
    size_t	len;

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

    memset(dest, '\01', sizeof(dest));

    sts = pmstrncpy(dest, destlen, argv[2]);
    printf("pmstrncpy(..., %d, \"%s\") -> %d dest=\"%s\"\n", (int)destlen, argv[2], sts, dest);
    len = strlen(argv[2]);
    for (i = len+1; i < sizeof(dest); i++) {
	if (dest[i] != '\01')
	    printf("Warning: dest[%d] over-written (%c \\0%o)\n", i, dest[i], dest[i]);
    }

    for (a = 3; a < argc; a++) {
	printf("pmstrncat(\"%s\", %d, \"%s\") -> ", dest, (int)destlen, argv[a]);
	sts = pmstrncat(dest, destlen, argv[a]);
	printf("%d dest=\"%s\"\n", sts, dest);
	len += strlen(argv[a]);
	for (i = len+1; i < sizeof(dest); i++) {
	    if (dest[i] != '\01')
		printf("Warning: dest[%d] over-written (%c \\0%o)\n", i, dest[i], dest[i]);
	}
    }

    exit(0);
}
