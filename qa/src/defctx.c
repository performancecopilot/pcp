/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * play with default contexts of various sorts
 */

#include <unistd.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

int
main(int argc, char **argv)
{
    int		c;
    int		ctx0 = -1;
    int		ctx1 = -1;
    int		sts;
    int		inst;
    int		errflag = 0;
    int		type = 0;
    char	*host = NULL;	/* pander to gcc */
    char	*namespace = PM_NS_DEFAULT;
    pmDesc	desc;
    pmID	pmid;
    const char	*name = "sample.colour";
    pmResult	*resp;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:n:s:")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmGetProgname());
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmGetProgname());
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s options ...\n\
\n\
Options:\n\
  -a archive	use archive, not host source\n\
  -D debugspec	set PCP debugging options\n\
  -h hostname	connect to PMCD on this host\n\
  -n namespace	alternative PMNS specification file\n",
		pmGetProgname());
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
	exit(1);
    }

    if (type != 0) {
	/* create an explicit context */
	if ((ctx0 = pmNewContext(type, host)) < 0) {
	    if (type == PM_CONTEXT_HOST)
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmGetProgname(), host, pmErrStr(sts));
	    else
		fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), host, pmErrStr(sts));
	    exit(1);
	}
    }

    /*
     * Add to profile, fetch, ...
     */
    if ((sts = pmLookupName(1, &name, &pmid)) < 0) {
	printf("%s: pmLookupName(%s): %s\n", pmGetProgname(), name, pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
	printf("%s: pmLookupDesc(%s): %s\n", pmGetProgname(), pmIDStr(pmid), pmErrStr(sts));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((inst = pmLookupInDomArchive(desc.indom, "green")) < 0) {
	    printf("%s: pmLookupInDomArchive(%s): %s\n", pmGetProgname(), pmInDomStr(desc.indom), pmErrStr(inst));
	    exit(1);
	}
    }
    else {
	if ((inst = pmLookupInDom(desc.indom, "green")) < 0) {
	    printf("%s: pmLookupInDom(%s): %s\n", pmGetProgname(), pmInDomStr(desc.indom), pmErrStr(inst));
	    exit(1);
	}
    }

    if ((sts = pmDelProfile(PM_INDOM_NULL, 1, (int *)0)) < 0) {
	printf("%s: pmDelProfile: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmAddProfile(desc.indom, 1, &inst)) < 0) {
	printf("%s: pmAddProfile: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetch(1, &pmid, &resp)) < 0) {
	printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
    }
    else {
	printf("first pmFetch is OK\n");
	__pmDumpResult(stdout, resp);
	pmFreeResult(resp);
    }
    
    /*
     * Now destroy context and try again, ... should see an invalid context
     */
    if ((sts = pmDestroyContext(ctx0)) < 0)
	printf("%s: pmDestroyContext: %s\n", pmGetProgname(), pmErrStr(sts));

    if ((sts = pmDelProfile(PM_INDOM_NULL, 1, (int *)0)) < 0) {
	printf("%s: pmDelProfile: %s\n", pmGetProgname(), pmErrStr(sts));
    }
    if ((sts = pmAddProfile(desc.indom, 1, &inst)) < 0) {
	printf("%s: pmAddProfile: %s\n", pmGetProgname(), pmErrStr(sts));
    }
    if ((sts = pmFetch(1, &pmid, &resp)) < 0) {
	printf("%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
    }
    else {
	printf("second pmFetch is OK\n");
	__pmDumpResult(stdout, resp);
	pmFreeResult(resp);
    }

    /*
     * destroy that one
     */
    if ((ctx0 = pmWhichContext()) < 0)
	printf("%s: pmWhichContext: %s\n", pmGetProgname(), pmErrStr(ctx0));
    else
	if ((sts = pmDestroyContext(ctx0)) < 0)
	    printf("%s: pmDestroyContext: %s\n", pmGetProgname(), pmErrStr(sts));

    if (type != 0) {
	/* play some more games */
	if ((ctx0 = pmNewContext(type, host)) < 0) {
	    if (type == PM_CONTEXT_HOST)
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmGetProgname(), host, pmErrStr(sts));
	    else
		fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), host, pmErrStr(sts));
	    exit(1);
	}
	printf("NewContext: %d\n", ctx0);
	if ((ctx1 = pmNewContext(type, host)) < 0) {
	    if (type == PM_CONTEXT_HOST)
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmGetProgname(), host, pmErrStr(sts));
	    else
		fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), host, pmErrStr(sts));
	    exit(1);
	}
	printf("NewContext: %d\n", ctx1);
	if ((sts = pmDestroyContext(ctx0)) < 0)
	    printf("%s: pmDestroyContext: %s\n", pmGetProgname(), pmErrStr(sts));
	else
	    printf("Destroy(%d)\n", ctx0);
	if ((sts = pmWhichContext()) < 0)
	    printf("%s: pmWhichContext: %s\n", pmGetProgname(), pmErrStr(sts));
	else
	    printf("WhichContext: %d\n", sts);

	if ((ctx0 = pmNewContext(type, host)) < 0) {
	    if (type == PM_CONTEXT_HOST)
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmGetProgname(), host, pmErrStr(sts));
	    else
		fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), host, pmErrStr(sts));
	    exit(1);
	}
	printf("NewContext: %d\n", ctx0);
	if ((sts = pmDestroyContext(ctx0)) < 0)
	    printf("%s: pmDestroyContext: %s\n", pmGetProgname(), pmErrStr(sts));
	else
	    printf("Destroy(%d)\n", ctx0);
	if ((sts = pmDestroyContext(ctx1)) < 0)
	    printf("%s: pmDestroyContext: %s\n", pmGetProgname(), pmErrStr(sts));
	else
	    printf("Destroy(%d)\n", ctx1);
    }

    exit(0);
}
