/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * Exercise pmdaCacheStoreInst() in libpcp_pmda
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

static int histo[128];

int
main(int argc, char **argv)
{
    int		key[3];
    int		inst;
    pmInDom	indom;
    int		i;
    int		j;
    int		k;
    int		s;
    int		c;
    int		n = 0;
    char	name[40];
    int		sts;
    int		errflag = 0;
    int		kflag = 0;
    char	*usage = "[-D debug] [-k]";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:k")) != EOF) {
	switch (c) {

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'k':	/* use key[], default is to use name[] */
	    kflag = 1;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc) {
	fprintf(stderr, "Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    indom = pmInDom_build(42, 42);

    /*
     * pattern of keys here is
     * - 32, 64 or 96 bits long
     * - features grouping and runs of values, as might be found in
     *   multi-dimensional indexes that need to be mapped to a  value
     *   no larger than 2^31 - 1 for an internal instance identifier
     */

    for (i = 0; i < 255; i++) {
	for (j = 1; j < 255; ) {
	    if (j == 0)
		key[0] = i;
	    else if (j == 1)
		key[0] = (i << 16) | j;
	    else if (j == 2)
		key[0] = (i << 24) | (i << 16) | j;
	    else
		key[0] = (i << 24) | (i << 16) | (j << 8) | j;
	    for (k = 0; k < 3; k++) {
		switch (k) {
		    case 0:
			snprintf(name, sizeof(name), "%08x", key[0]);
			break;
		    case 1:
			key[1] = key[0];
			snprintf(name, sizeof(name), "%08x-%08x", key[0], key[1]);
			break;
		    case 2:
			key[2] = key[1] = key[0];
			snprintf(name, sizeof(name), "%08x-%08x-%08x", key[0], key[1], key[2]);
			break;
		    case 3:
			key[2] = key[1] = key[0];
			key[3]++;
			snprintf(name, sizeof(name), "%08x-%08x-%08x-%08x", key[0], key[1], key[2], key[3]);
			break;
		}

		if (kflag)
		    inst = pmdaCacheStoreInst(indom, PMDA_CACHE_ADD, name, k+1, key, NULL);
		else
		    inst = pmdaCacheStoreInst(indom, PMDA_CACHE_ADD, name, 0, NULL, NULL);
		if (inst < 0) {
		    printf("pmdaCacheStoreInst failed: %s\n", pmErrStr(inst));
		    exit(1);
		}
		if (kflag) {
		    printf("%d <-", inst);
		    for (c = 0; c <= k; c++)
			printf(" %d", key[k]);
		    putchar('\n');
		}
		else
		    printf("%d <- %s\n", inst, name);
		histo[(int)(inst/(0x7fffffff/128))]++;
		n++;
	    }
	    if (j < 32)
		j++;
	    else
		j = 2*j + 1;
	}
    }

    for (s = 1; s <= 64; s *= 2) {
	printf("\nInstances distribution across %d bins\n", 128/s);
	c = 0;
	for (j = 0; j < 128; j += s) {
	    i = 0;
	    for (k = 0; k < s; k++)
		i += histo[j+k];
	    printf("%.5f ", (double)i/n);
	    c++;
	    if (c == 10) {
		putchar('\n');
		c = 0;
	    }
	}
	putchar('\n');
    }

    return 0;
}
