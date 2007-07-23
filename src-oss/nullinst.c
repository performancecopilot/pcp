/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ident "$Id: nullinst.c,v 1.1 2002/10/22 07:02:33 kenmcd Exp $"

/*
 * nullinst - check metrics with indom PM_INDOM_NULL return 1
 * value with inst == PM_IN_NULL
 */

#include <stdlib.h>
#include <errno.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

static void
dometric(const char *name)
{
    pmID	pmid;
    int		n;
    pmDesc	desc;
    pmResult	*rp;

    if ((n = pmLookupName(1, (char **)&name, &pmid)) < 0) {
	printf("pmLookupName(%s): %s\n", name, pmErrStr(n));
	return;
    }
    if ((n = pmLookupDesc(pmid, &desc)) < 0) {
	printf("pmLookupDesc(%s): %s\n", name, pmErrStr(n));
	return;
    }
    if (desc.indom != PM_INDOM_NULL)
	return;

    if ((n = pmFetch(1, &pmid, &rp)) < 0) {
	printf("pmFetch(%s): %s\n", name, pmErrStr(n));
	return;
    }

    if (rp->numpmid == 1) {
	if (rp->vset[0]->numval == 1) {
	    if (rp->vset[0]->vlist[0].inst != PM_IN_NULL)
		printf("%s: bad inst (%d)\n", name, rp->vset[0]->vlist[0].inst);
	}
	/* ignore errors from unsupported metrics on this platform */
	else if (rp->vset[0]->numval != PM_ERR_APPVERSION &&
		 rp->vset[0]->numval != PM_ERR_AGAIN &&
		 rp->vset[0]->numval != -ENOPKG)
	    printf("%s: bad numval (%d)\n", name, rp->vset[0]->numval);
    }
    else
	printf("%s: bad numpmid (%d)\n", name, rp->numpmid);

    pmFreeResult(rp);

}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    char	*p;
    int		errflag = 0;
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
    static char	*usage = "[-n namespace] metric ...";
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, "D:n:")) != EOF) {
	switch (c) {
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

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind == argc) {
	printf("Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    if ((sts = pmLoadNameSpace(namespace)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
	printf("%s: Cannot connect to PMCD on host \"%s\": %s\n", pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    for ( ; optind < argc; optind++)
	pmTraversePMNS(argv[optind], dometric);

    exit(0);
    /*NOTREACHED*/
}
