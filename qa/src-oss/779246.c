/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * pv 779246 - check sample pmda returns type on sample.sysinfo
 */

#include <ctype.h>
#include <string.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

#ifdef HAVE_SYSINFO
#include <sys/sysinfo.h>

#define NUM 1

static int
dometric(void)
{
    int			n;
    pmID		pmid;
    pmDesc		desc;
    pmResult	*result;

    char 		*namelist[NUM]		={ "sample.sysinfo" };
    pmID		pmidlist[NUM];

    if ((n = pmLookupName(NUM, namelist, pmidlist)) < 0) {
	printf("pmLookupDesc: %s\n", pmErrStr(n));
	return 1;
    }

    pmid=pmidlist[0];

    if ((n = pmLookupDesc(pmid, &desc)) < 0) {
	printf("pmLookupDesc: %s\n", pmErrStr(n));
	return 1;
    }
    __pmPrintDesc(stdout, &desc);

    if ((n = pmFetch(1, &pmid, &result)) < 0)
	printf("pmFetch: %s\n", pmErrStr(n));

    if (result->numpmid !=1 ) {
	printf("pmFetch, numpmid == %d (1)\n",
			result->numpmid);
	return 1;
    }

    if (result->vset[0]->numval < 0) {
	printf("pmFetch, numval: %s\n", pmErrStr(result->vset[0]->numval));
	return 1;
    } else if (result->vset[0]->numval != 1) {
    	printf("pmFetch, numval: == %d (1)\n", 
		    result->vset[0]->numval);
	return 1;
    }

    if (result->vset[0]->valfmt==PM_VAL_INSITU) {
	printf("sample.sysinfo is in-situ !?\n");
	return 1;
    }
    printf("sample.sysinfo vtype == %d\n",
	    result->vset[0]->vlist[0].value.pval->vtype);

#if PMAPI_VERSION == 2
    __pmDumpResult(stdout, result);
#else
    _pmDumpResult(stdout, result);
#endif


    return 0;
}

int
main(int argc, char *argv[])
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
#ifdef PCP_DEBUG
    static char	*debug = "[-D N] ";
#else
    static char	*debug = "";
#endif
    static char	*usage = "[-h hostname]";

    __pmSetProgname(pmProgname);

    while ((c = getopt(argc, argv, "D:h:")) != EOF) {
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

	case 'h':	/* hostname for PMCD to contact */
	    host = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc) {
	fprintf(stderr, "Usage: %s %s%s\n", pmProgname, debug, usage);
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

    /* Size of struct sysinfo is different on different platforms, so
     * give a hint how many bytes are expected in the output */
    printf("+++ Expect %d bytes\n", (int)sizeof(struct sysinfo));
    return dometric();
}
#else
int
main(int argc, char **argv)
{
    printf("No sysinfo on this platform\n");
    exit(1);
}
#endif /* HAVE_SYSINFO */
