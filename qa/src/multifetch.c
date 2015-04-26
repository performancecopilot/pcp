/*
 * Copyright (c) 1994-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <unistd.h>
#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    char	*cmd = argv[0];
    int		errflag = 0;
    int		type = 0;
    char	*host = NULL;			/* pander to gcc */
    char 	*logfile = (char *)0;
    pmLogLabel	label;				/* get hostname for archives */
    char	local[MAXHOSTNAMELEN];
    char	*namespace = PM_NS_DEFAULT;
    int		samples = 1000;
    double	delta = 1.0;
    int		i;
    int		j;
    char	*namelist[20];
    pmID	pmidlist[20];
    int		n;
    int		numpmid;
    pmResult	*resp;
    char	*endnum;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:l:n:s:t:T:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", cmd);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

#ifdef DEBUG

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
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", cmd);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'l':	/* logfile */
	    logfile = optarg;
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", cmd);
		errflag++;
	    }
	    break;

	case 't':	/* delta seconds (double) */
	    delta = strtod(optarg, &endnum);
	    if (*endnum != '\0' || delta <= 0.0) {
		fprintf(stderr, "%s: -t requires floating point argument\n", cmd);
		errflag++;
	    }
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
Options\n\
  -a   archive	  metrics source is an archive log\n\
  -D   debug	  standard PCP debug flag\n\
  -h   host	  metrics source is PMCD on host\n\
  -l   logfile	  redirect diagnostics and trace output\n\
  -n   namespace  use an alternative PMNS\n\
  -s   samples	  terminate after this many iterations\n\
  -t   delta	  sample interval in seconds(float) [default 1.0]\n",
		cmd);
	exit(1);
    }

    if (logfile != (char *)0) {
	__pmOpenLog(cmd, logfile, stderr, &sts);
	if (sts < 0) {
	    fprintf(stderr, "%s: Could not open logfile\n", pmProgname);
	}
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", cmd, namespace, pmErrStr(sts));
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
		cmd, host, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		cmd, host, pmErrStr(sts));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		cmd, pmErrStr(sts));
	    exit(1);
	}
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    while (optind < argc) {
	printf("extra argument[%d]: %s\n", optind, argv[optind]);
	optind++;
    }

    i = 0;
    /* we just want metrics from different pmdas here. choose ones
        supported on both linux & irix */
    namelist[i++] = "network.interface.in.bytes";       
    namelist[i++] = "network.interface.in.packets";
    namelist[i++] = "network.interface.out.errors";
    namelist[i++] = "network.interface.out.packets";
    namelist[i++] = "simple.color";
    namelist[i++] = "simple.time.user";
    namelist[i++] = "simple.numfetch";
    namelist[i++] = "sample.step";
    namelist[i++] = "sample.lights";
    namelist[i++] = "sample.magnitude";
    numpmid = i;

    n = pmLookupName(numpmid, namelist, pmidlist);
    if (n < 0) {
	fprintf(stderr, "pmLookupName: %s\n", pmErrStr(n));
	exit(1);
    }
    if (n != numpmid) {
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		printf("	%s - not known\n", namelist[i]);
	}
	exit(1);
    }

    for (j = 0; j < samples; j++) {
	pmDesc	desc;
	for (i = 0; i < numpmid; i++) {
	    if ((n = pmLookupDesc(pmidlist[i], &desc)) < 0) {
		fprintf(stderr, "pmFetch: %s\n", pmErrStr(n));
		exit(1);
	    }
	}
	for (i = 0; i < 5; i++) {
	    if ((n = pmFetch(numpmid, pmidlist, &resp)) < 0) {
		fprintf(stderr, "pmFetch: %s\n", pmErrStr(n));
		exit(1);
	    }
	    pmFreeResult(resp);
	}
    }

    exit(0);
}
