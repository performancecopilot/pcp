/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * check open/close archive for mem leaks
 */

#include <sys/time.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

static int	vflag = 0;
static int	tflag = 0;

int
main(int argc, char **argv)
{
    int		i, c;
    int		sts;
    int		errflag = 0;
    char	*archive = "foo";
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
#ifdef PCP_DEBUG
    static char	*debug = "[-D N]";
#else
    static char	*debug = "";
#endif
    static char	*usage = "[-D N] [-L] [-h host] [-a archive] [-n namespace] [-v] [-i iterations]";
    int		niter = 100;
    int		contype = PM_CONTEXT_HOST;
    unsigned long first_memusage;
    unsigned long last_memusage = 0;
    unsigned long memusage;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "Li:h:a:D:n:tv")) != EOF) {
	switch (c) {

	case 'i':	/* iterations */
	    niter = atoi(optarg);
	    break;
	case 'L': 	/* local */
	    contype = PM_CONTEXT_LOCAL;
	    break;
	case 'h':	/* host */
	    host = optarg;
	    contype = PM_CONTEXT_HOST;
	    break;

	case 'a':	/* archive */
	    archive = optarg;
	    contype = PM_CONTEXT_ARCHIVE;
	    break;

#ifdef PCP_DEBUG
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
#endif

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 'v':	/* verbose output */
	    vflag++;
	    break;

	case 't':	/* trim namespace */
	    tflag++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	printf("Usage: %s %s%s\n", pmProgname, debug, usage);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT) {
	if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	    printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	    exit(1);
	}
    }

    for (i = 0; i < niter; i++) {
	switch (contype) {
	case PM_CONTEXT_LOCAL:
	    if ((c = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0) {
		printf("%s: Cannot create local context: %s\n", pmProgname, pmErrStr(c));
		exit(1);
	    }
	    break;
	case PM_CONTEXT_ARCHIVE:
	    if ((c = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0) {
		printf("%s: Cannot connect to archive \"%s\": %s\n", pmProgname, archive, pmErrStr(c));
		exit(1);
	    }
	    break;
	case PM_CONTEXT_HOST:
	    if ((c = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
		printf("%s: Cannot connect to host \"%s\": %s\n", pmProgname, host, pmErrStr(c));
		exit(1);
	    }
	    break;
	}

	pmDestroyContext(c);
	if (i == 0) {
	    __pmProcessDataSize(&first_memusage);
	}
	else {
	    __pmProcessDataSize(&memusage);
	    if (memusage - first_memusage > 0) {
		if (i > 1)
		    printf("iteration %d: leaked %lu bytes\n", i,
			   memusage - last_memusage);
		last_memusage = memusage;
	    }
	}
    }

    exit(0);
}

