/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * check open/close archive for mem leaks
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

#include "localconfig.h"

static int	vflag;
static int	tflag;

void
do_PMNS_op(char *msg)
{
    int sts;
    char *name = "pmcd.control.debug";
    pmID pmid;

    printf("---%s---\n", msg);
    printf("PMNS location = %d\n", pmGetPMNSLocation());
    if ((sts = pmLookupName(1, &name, &pmid)) < 0) { 
	fprintf(stderr, "%s: lookup failed: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    } 
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*archive = "foo";
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
    static char	*usage = "[-D N] [-L] [-h host] [-a archive] [-n namespace] [-v] [-i iterations]";
    int		niter=100;
    int		i;
    int		contype = PM_CONTEXT_HOST;

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
	printf("Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    switch (contype) {
    case PM_CONTEXT_LOCAL:
	if ((sts = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0) {
	    printf("%s: Cannot create local context: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
	break;
    case PM_CONTEXT_ARCHIVE:
	if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	    printf("%s: Cannot connect to archive \"%s\": %s\n", pmProgname, archive, pmErrStr(sts));
	    exit(1);
	}
	break;
    case PM_CONTEXT_HOST:
	if ((sts = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
	    printf("%s: Cannot connect to host \"%s\": %s\n", pmProgname, host, pmErrStr(sts));
	    exit(1);
	}
	break;
    }

    for (i=0; i < niter; i++) {
	printf("***iteration %d***\n", i);
	do_PMNS_op("pre-unload");
	pmUnloadNameSpace();
	do_PMNS_op("post-unload");
	if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	    printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	    exit(1);
	}
	do_PMNS_op("post-load");
    }
    pmUnloadNameSpace();

    (void)pmDestroyContext(pmWhichContext());

#if PCP_VER >= 3611
    __pmShutdown();
#endif

    exit(0);
}

