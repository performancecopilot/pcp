/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>

/* Be careful when changing LIMIT.  Also change malloc and pmsprintf! */

#define LIMIT 10000			/* max nfiles allowed */

static void
usage (void)
{
    fprintf(stderr, "Usage %s: basename nfiles\n", pmGetProgname());
    exit(1);
}

int
main(int argc, char* argv[])
{
    char	*endp;
    long	nfiles;
    char	*namebuf;
    char	*extptr;
    int		i, sts, len;

    pmSetProgname(argv[0]);

    if (argc != 3)
	usage();

    nfiles = strtol(argv[2], &endp, 0);
    if (*endp != '\0') {
	fprintf(stderr, "nfiles \"%s\" is not numeric\n", argv[2]);
	usage();
    }
    if (nfiles > LIMIT) {
	fprintf(stderr, "be reasonable: nfiles limited to %d\n", LIMIT);
	usage();
    }

    i = (int)strlen(argv[1]);
    len = i + 6;
    namebuf = (char *)malloc(len);
    if (namebuf == (char *)0) {
	perror("error allocating filename buffer");
	exit(1);
    }
    strcpy(namebuf, argv[1]);
    namebuf[i++] = '.';
    extptr = &namebuf[i];
    len -= i;

    for (i = 0; i < nfiles; i++) {
	pmsprintf(extptr, len, "%04d", i);
	if ((sts = creat(namebuf, 0777)) < 0) {
	    fprintf(stderr, "Error creating %s: %s\n", namebuf, strerror(errno));
	    exit(1);
	}
	else
	    close(sts);
    }
    exit(0);
}
