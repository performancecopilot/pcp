/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * Exercise pmdaCacheStoreKey() in libpcp_pmda
 */

#include <pcp/pmapi.h>
#include <pcp/pmda.h>

static int histo[128];
static int vflag = 0;

static void
do_key(int i, int j, int k, char *name, int namelen, int *keylen, int *key)
{
    if (j == 1)
	key[0] = (i << 8) | j;
    else if (j == 2)
	key[0] = (i << 16) | (j << 8) | k;
    else
	key[0] = (i << 24) | (j << 16) | (k << 8) | i;
    *keylen = (k+1)*sizeof(int);
    switch (k) {
	case 0:
	    pmsprintf(name, namelen, "%08x", key[0]);
	    break;
	case 1:
	    key[1] = i;
	    pmsprintf(name, namelen, "%08x-%08x", key[0], key[1]);
	    break;
	case 2:
	    key[1] = i;
	    key[2] = j;
	    pmsprintf(name, namelen, "%08x-%08x-%08x", key[0], key[1], key[2]);
	    break;
	case 3:
	    key[1] = i;
	    key[2] = j;
	    key[3] = k;
	    pmsprintf(name, namelen, "%08x-%08x-%08x-%08x", key[0], key[1], key[2], key[3]);
	    break;
    }

    if (vflag)
	fprintf(stderr, "do_key(%d, %d, %d, ...) -> %s\n", i, j, k, name);
}

/*
 * Assume we're following -d or -dk, so dealing with the smaller
 * set of keys the -d implies
 */
static void
load_n_go(pmInDom indom)
{
    int		sts;
    int		keylen;
    int		key[4];	
    char	name[40];
    int		i;
    int		j;
    int		k;
    int		kflag;
    int		inst;

    sts = pmdaCacheOp(indom, PMDA_CACHE_LOAD);
    if (sts < 0)
	fprintf(stderr, "pmdaCacheOp(%s, PMDA_CACHE_LOAD) failed: %s\n", pmInDomStr(indom), pmErrStr(sts));
    fprintf(stderr, "Cache loaded ...\n");
    pmdaCacheOp(indom, PMDA_CACHE_DUMP);

    for (i = 0; i < 5; i++) {	/* one more iteration than -d in main() */
	for (j = 1; j < 5; ) {	/* one more iteration than -d in main() */
	    for (k = 0; k < 4; k++) {	/* one more iteration than -d in main() */
		do_key(i, j, k, name, sizeof(name), &keylen, key);
		for (kflag = 1; kflag >= 0; kflag--) {
		    /* force duplicate keys by culling the key[] or name[] */
		    if (kflag)
			keylen = 4;
		    else
			name[13] = '\0';

		    if (kflag) {
			int		c;
			for (c = 0; c < keylen/sizeof(int); c++)
			    key[c] = htonl(key[c]);
			inst = pmdaCacheStoreKey(indom, PMDA_CACHE_ADD, name, keylen, (const void *)key, NULL);
			for (c = 0; c < keylen/sizeof(int); c++)
			    key[c] = ntohl(key[c]);
		    }
		    else
			inst = pmdaCacheStoreKey(indom, PMDA_CACHE_ADD, name, 0, NULL, NULL);
		    if (kflag) {
			int		c;
			fprintf(stderr, "%d <- %s ", inst, name);
			for (c = 0; c < keylen/sizeof(int); c++) {
			    if (c == 0)
				fprintf(stderr, "[%d", key[c]);
			    else
				fprintf(stderr, ",%d", key[c]);
			}
			fputc(']', stderr);
			fputc('\n', stderr);
		    }
		    else
			fprintf(stderr, "%d <- %s\n", inst, name);
		    if (inst < 0)
			fprintf(stderr, "pmdaCacheStoreKey failed: %s\n", pmErrStr(inst));
		}
	    }
	    j++;
	}
    }
    pmdaCacheOp(indom, PMDA_CACHE_DUMP_ALL);
}

int
main(int argc, char **argv)
{
    int		key[4];		/* key[3] not used as yet */
    int		inst;
    pmInDom	indom;
    int		i;
    int		j;
    int		k;
    int		keylen;
    int		s;
    int		c;
    int		n = 0;
    char	name[40];
    int		sts;
    int		errflag = 0;
    int		kflag = 0;
    int		dflag = 0;
    int		lflag = 0;
    char	*usage = "[-D debug] [-dkl]";

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:dklv")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'd':	/* exercise duplicate checking, save, smaller set of keys */
	    dflag = 1;
	    break;

	case 'k':	/* use key[], default is to use name[] */
	    kflag = 1;
	    break;

	case 'l':	/* load */
	    lflag = 1;
	    break;

	case 'v':	/* verbose */
	    vflag = 1;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc) {
	fprintf(stderr, "Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    indom = pmInDom_build(42, 42);

    if (lflag) {
	load_n_go(indom);
	exit(0);
    }

    /*
     * pattern of keys here is
     * - 32, 64 or 96 bits long
     * - features grouping and runs of values, as might be found in
     *   multi-dimensional indexes that need to be mapped to a  value
     *   no larger than 2^31 - 1 for an internal instance identifier
     */

    for (i = 0; i < 255; i++) {
	for (j = 1; j < 255; ) {
	    for (k = 0; k < 3; k++) {
		do_key(i, j, k, name, sizeof(name), &keylen, key);

		if (dflag) {
		    /* force duplicate keys by culling the key[] or name[] */
		    if (kflag)
			keylen = 4;
		    else
			name[13] = '\0';
		}

		if (kflag) {
		    int		c;
		    for (c = 0; c < keylen/sizeof(int); c++)
			key[c] = htonl(key[c]);
		    inst = pmdaCacheStoreKey(indom, PMDA_CACHE_ADD, name, keylen, (const void *)key, NULL);
		    for (c = 0; c < keylen/sizeof(int); c++)
			key[c] = ntohl(key[c]);
		}
		else
		    inst = pmdaCacheStoreKey(indom, PMDA_CACHE_ADD, name, 0, NULL, NULL);
		if (kflag) {
		    fprintf(stderr, "%d <- %s ", inst, name);
		    for (c = 0; c < keylen/sizeof(int); c++) {
			if (c == 0)
			    fprintf(stderr, "[%d", key[c]);
			else
			    fprintf(stderr, ",%d", key[c]);
		    }
		    fputc(']', stderr);
		    fputc('\n', stderr);
		}
		else
		    fprintf(stderr, "%d <- %s\n", inst, name);
		if (inst < 0) {
		    fprintf(stderr, "pmdaCacheStoreKey failed: %s\n", pmErrStr(inst));
		    continue;
		}
		histo[(int)(inst/(0x7fffffff/128))]++;
		n++;
	    }
	    if (dflag && j == 3)
		break;
	    if (j < 32)
		j++;
	    else
		j = 2*j + 1;
	}
	if (dflag && i == 3) {
	    pmdaCacheOp(indom, PMDA_CACHE_DUMP_ALL);
	    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
	    if (sts < 0)
		fprintf(stderr, "pmdaCacheOp(%s, PMDA_CACHE_SAVE) failed: %s\n", pmInDomStr(indom), pmErrStr(sts));
	    sts = pmdaCacheOp(indom, PMDA_CACHE_CULL);
	    if (sts < 0)
		fprintf(stderr, "pmdaCacheOp(%s, PMDA_CACHE_CULL) failed: %s\n", pmInDomStr(indom), pmErrStr(sts));
	    exit(0);
	}
    }

    for (s = 1; s <= 64; s *= 2) {
	fprintf(stderr, "\nInstances distribution across %d bins\n", 128/s);
	c = 0;
	for (j = 0; j < 128; j += s) {
	    i = 0;
	    for (k = 0; k < s; k++)
		i += histo[j+k];
	    fprintf(stderr, "%.5f ", (double)i/n);
	    c++;
	    if (c == 10) {
		fputc('\n', stderr);
		c = 0;
	    }
	}
	fputc('\n', stderr);
    }

    return 0;
}
