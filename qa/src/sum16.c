/*
 * Copyright (c) 2016 Ken McDonell.  All Rights Reserved.
 *
 * BSD sum(1) replacement.
 *
 * Algorithm reference: https://en.wikipedia.org/wiki/BSD_checksum
 */
#include <stdio.h>

static
int chksum(FILE *fp, int *bp)
{
    int		c;
    long	bytes = 0;
    int		sum = 0;

    while ((c = fgetc(fp)) != EOF) {
	sum = (sum >> 1) + ((sum & 1) << 15);
	sum = (sum + c ) & 0xffff;
	bytes++;
    }
    *bp = (bytes + 1023) / 1024;

    return sum;
}

int
main(int argc, char **argv)
{
    int		sum;
    int		blocks;
    FILE	*fp;

    if (argc == 1) {
	sum = chksum(stdin, &blocks);
	printf("%05d %5d\n", sum, blocks);
    }
    else {
	int	i;
	for (i = 1; i < argc; i++) {
	    fp = fopen(argv[i], "r");
	    if (fp == NULL) {
		fprintf(stderr, "%s: fopen failed\n", argv[i]);
	    }
	    else {
		sum = chksum(fp, &blocks);
		printf("%05d %5d %s\n", sum, blocks, argv[i]);
		fclose(fp);
	    }
	}
    }

    return(0);
}
