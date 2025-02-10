/*
 * juggle with the PMDA's indom cache
 */

#include <pcp/pmapi.h>
#include <pcp/pmda.h>

int
main(int argc, char **argv)
{
    int 	errflag = 0;
    pmInDom	indom;
    int		sts;
    int 	c;
    int		domain;
    int		serial;

    indom = pmInDom_build(0, 123);

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "Cc:D:df:h:LSs:")) != EOF) {
	switch (c) {

	case 'C':
	    sts = pmdaCacheOp(indom, PMDA_CACHE_CULL);
	    fprintf(stderr, "cull(%s) -> %d", pmInDomStr(indom), sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'c':
	    sts = pmdaCacheStore(indom, PMDA_CACHE_CULL, optarg, NULL);
	    fprintf(stderr, "cull(%s) -> %d", optarg, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'd':
#ifdef PMDA_CACHE_DUMP_ALL
	    sts = pmdaCacheOp(indom, PMDA_CACHE_DUMP_ALL);
#else
	    sts = pmdaCacheOp(indom, PMDA_CACHE_DUMP);
#endif
	    break;

	case 'f':
	    sscanf(optarg, "%d.%d", &domain, &serial);
	    indom = pmInDom_build(domain, serial);
	    sts = pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	    fprintf(stderr, "load(%s) -> %d", pmInDomStr(indom), sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'h':
	    sts = pmdaCacheStore(indom, PMDA_CACHE_HIDE, optarg, NULL);
	    fprintf(stderr, "hide(%s) -> %d", optarg, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'L':
	    sts = pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	    fprintf(stderr, "load(%s) -> %d", pmInDomStr(indom), sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'S':
	    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
	    fprintf(stderr, "save(%s) -> %d", pmInDomStr(indom), sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 's':
	    sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, optarg, NULL);
	    fprintf(stderr, "store(%s) -> %d", optarg, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s ...\n", pmGetProgname());
	fprintf(stderr, "options:\n");
	fprintf(stderr, "-C             cull all\n");
	fprintf(stderr, "-c inst        cull one inst\n");
	fprintf(stderr, "-D debug\n");
	fprintf(stderr, "-d             dump\n");
	fprintf(stderr, "-f dom.serial  use saved indom from $PCP_VAR_CONFIG/pmda/dom.serial\n");
	fprintf(stderr, "               (implies -L)\n");
	fprintf(stderr, "-h inst        hide\n");
	fprintf(stderr, "-L             load\n");
	fprintf(stderr, "-S             store\n");
	fprintf(stderr, "-s inst        add one inst\n");
	exit(1);
    }

    return 0;
}

