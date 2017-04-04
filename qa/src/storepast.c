/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * storepast - illegal pmStore wrt to current
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    char	*host = NULL;			/* pander to gcc */
    char	local[MAXHOSTNAMELEN];
    char	*namespace = PM_NS_DEFAULT;
    pmResult	req;
    char	*name = "sample.write_me";
    pmID	pmid;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:n:")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
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

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
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
  -a archive	use archive log, not host source\n\
  -D N		set pmDebug debugging flag to N\n\
  -h hostname	connect to PMCD on this host\n\
  -n namespace	alternative PMNS specification file\n",
		pmProgname);
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

    if (type == 0) {
	type = PM_CONTEXT_HOST;
	gethostname(local, sizeof(local));
	host = local;
    }
    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if (type == PM_CONTEXT_HOST) {
	struct timeval	back;
	gettimeofday(&back, (struct timezone *)0);
	back.tv_sec -= 3600;	/* an hour ago */
	if ((sts = pmSetMode(PM_MODE_BACK, &back, 0)) < 0) {
	    printf("%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }

    if ((sts = pmLookupName(1, &name, &pmid)) < 0) {
	printf("%s: pmLookupName: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    req.numpmid = 1;
    req.vset[0] = (pmValueSet *)malloc(sizeof(pmValueSet));
    req.vset[0]->pmid = pmid;
    req.vset[0]->numval = 1;
    req.vset[0]->valfmt = PM_VAL_INSITU;
    req.vset[0]->vlist[0].inst = PM_IN_NULL;
    req.vset[0]->vlist[0].value.lval = 1;

    sts = pmStore(&req);

    printf("pmStore, should produce PM_ERR_NOHOST, ...\n%s\n", pmErrStr(sts));

    exit(0);
}
