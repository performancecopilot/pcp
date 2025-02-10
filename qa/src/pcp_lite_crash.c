/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * pcp_lite_crash - crash pcp lite
 */

#include <pcp/pmapi.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	local[MAXHOSTNAMELEN];
    char	*namespace = PM_NS_DEFAULT;
    pmID	pmid;
    pmDesc	desc;
    int		type = 0;
    char	*host = NULL;
    const char	*metric = "kernel.all.cpu.idle";

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:Ln:")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmGetProgname());
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
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmGetProgname());
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'L':	/* local, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmGetProgname());
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
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
  -L		connect local, no PMCD\n\
  -n namespace	alternative PMNS specification file\n",
		pmGetProgname());
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
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
		pmGetProgname(), host, pmErrStr(sts));
	else if (type == PM_CONTEXT_LOCAL)
	    fprintf(stderr, "%s: Cannot connect in local standalone mode: %s\n",
		pmGetProgname(), pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmGetProgname(), host, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmLookupName(1, &metric, &pmid)) < 0) {
        printf("%s: %s\n", pmGetProgname(), pmErrStr(sts));
        exit(1);
    }

    /* crash in here: dereference a null pointer to function */
    if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
        fprintf(stderr, "pmLookupDesc: %s\n", pmErrStr(sts));
        exit(1);
    }

    exit(0);
}
