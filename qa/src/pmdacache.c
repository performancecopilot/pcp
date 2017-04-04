#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

int
main(int argc, char **argv)
{
    int errflag = 0;
    int	indom = 123;
    int	sts;
    int c;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "Cc:D:dh:LSs:")) != EOF) {
	switch (c) {

	case 'C':
	    sts = pmdaCacheOp(indom, PMDA_CACHE_CULL);
	    fprintf(stderr, "cull() -> %d", sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'c':
	    sts = pmdaCacheStore(indom, PMDA_CACHE_CULL, optarg, NULL);
	    fprintf(stderr, "cull(%s) -> %d", optarg, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

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

	case 'd':
#ifdef PMDA_CACHE_DUMP_ALL
	    sts = pmdaCacheOp(indom, PMDA_CACHE_DUMP_ALL);
#else
	    sts = pmdaCacheOp(indom, PMDA_CACHE_DUMP);
#endif
	    break;

	case 'h':
	    sts = pmdaCacheStore(indom, PMDA_CACHE_HIDE, optarg, NULL);
	    fprintf(stderr, "hide(%s) -> %d", optarg, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'L':
	    sts = pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	    fprintf(stderr, "load() -> %d", sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'S':
	    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
	    fprintf(stderr, "save() -> %d", sts);
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
	fprintf(stderr, "Usage: %s ...\n", pmProgname);
	fprintf(stderr, "options:\n");
	fprintf(stderr, "-C             cull all\n");
	fprintf(stderr, "-c inst        cull one\n");
	fprintf(stderr, "-D debug\n");
	fprintf(stderr, "-d             dump\n");
	fprintf(stderr, "-h inst        hide\n");
	fprintf(stderr, "-L             load\n");
	fprintf(stderr, "-S             store\n");
	fprintf(stderr, "-s inst        save\n");
	exit(1);
    }

    return 0;
}

