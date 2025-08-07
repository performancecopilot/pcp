/*
 * Copyright (c) 1994-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <unistd.h>
#include <ctype.h>
#include <pcp/pmapi.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    char	*cmd = argv[0];
    int		errflag = 0;
    int		highres = 0;
    int		samples = 1000;
    int		iterations = 5;
    int		type = 0;
    char	*host = NULL;			/* pander to gcc */
    char 	*logfile = (char *)0;
    char	*namespace = PM_NS_DEFAULT;
    int		i;
    int		j;
    const char	*namelist[20];
    pmID	pmidlist[20];
    int		n;
    int		numpmid;
    pmDesc	desc;
    pmResult	*resp;
    pmResult *hresp;
    char	*endnum;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:Hl:n:s:T:?")) != EOF) {
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

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;
#endif

	case 'H':	/* high resolution sample times */
	    highres = 1;
	    break;

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

	case 'i':	/* iteration count (inner loop) */
	    iterations = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -i requires numeric argument\n", cmd);
		errflag++;
	    }
	    break;

	case 's':	/* sample count (outer loop) */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", cmd);
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
  -a   archive	  metrics source is an archive\n\
  -D   debugspec  standard PCP debugging options\n\
  -h   host	  metrics source is PMCD on host\n\
  -H              use high resolution timestamp samples\n\
  -l   logfile	  redirect diagnostics and trace output\n\
  -n   namespace  use an alternative PMNS\n\
  -i   iterations inner loop multiplier iteration count [5]\n\
  -s   samples	  terminate after this many outer loops [1000]\n",
		cmd);
	exit(1);
    }

    if (logfile != (char *)0) {
	pmOpenLog(cmd, logfile, stderr, &sts);
	if (sts != 1) {
	    fprintf(stderr, "%s: Could not open logfile\n", pmGetProgname());
	}
    }

    if ((namespace != PM_NS_DEFAULT) &&
	(sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	fprintf(stderr, "%s: Cannot load namespace from \"%s\": %s\n", cmd,
		namespace, pmErrStr(sts));
	exit(1);
    }

    if (type == 0) {
	type = PM_CONTEXT_HOST;
	host = "local:";
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
	for (i = 0; i < numpmid; i++) {
	    if ((n = pmLookupDesc(pmidlist[i], &desc)) < 0) {
		fprintf(stderr, "pmLookupDesc: %s\n", pmErrStr(n));
		exit(1);
	    }
	}
	if (highres) {
	    for (i = 0; i < iterations; i++) {
		if ((n = pmFetch(numpmid, pmidlist, &hresp)) < 0) {
		    fprintf(stderr, "pmFetch: %s\n", pmErrStr(n));
		    exit(1);
		}
		pmFreeResult(hresp);
	    }
	} else {
	    for (i = 0; i < iterations; i++) {
		if ((n = pmFetch(numpmid, pmidlist, &resp)) < 0) {
		    fprintf(stderr, "pmFetch: %s\n", pmErrStr(n));
		    exit(1);
		}
		pmFreeResult(resp);
	    }
	}
    }

    exit(0);
}
