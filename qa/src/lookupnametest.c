/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2015 Martins Innus.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

#define BUILD_STANDALONE 1

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    int		mode = PM_MODE_INTERP;		/* mode for archives */
    int		i;
    int		numpmid;
    char	*host = NULL;			/* pander to gcc */
    char	*pmnsfile = PM_NS_DEFAULT;
    char	**namelist;
    pmID	*pmidlist;
    char	*name;

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:Ln:x?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
#ifdef BUILD_STANDALONE
		fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n", pmProgname);
#else
		fprintf(stderr, "%s: at most one of -a, -h and -x allowed\n", pmProgname);
#endif
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    printf("Using archive context: %s\n", host);
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

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
#ifdef BUILD_STANDALONE
		fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n", pmProgname);
#else
		fprintf(stderr, "%s: at most one of -a, -h and -x allowed\n", pmProgname);
#endif
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    printf("Using host context: %s\n", host);
	    break;

#ifdef BUILD_STANDALONE
	case 'L':	/* LOCAL, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n", pmProgname);
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
	    putenv("PMDA_LOCAL_PROC=");		/* if proc PMDA needed */
	    putenv("PMDA_LOCAL_SAMPLE=");	/* if sampledso PMDA needed */
	    printf("Using PM_CONTEXT_LOCAL context\n");
	    break;
#endif

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    printf("Using PMNS: %s\n", pmnsfile);
	    break;

	case 'x':	/* no pmNewContext */
	    if (type != 0) {
#ifdef BUILD_STANDALONE
		fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n", pmProgname);
#else
		fprintf(stderr, "%s: at most one of -a, -h and -x allowed\n", pmProgname);
#endif
		errflag++;
	    }
	    type = -1;
	    printf("Using no context\n");
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] metricname ...\n\
\n\
Options:\n\
  -a archive     metrics source is a PCP log archive\n\
  -h host        metrics source is PMCD on host\n"
#ifdef BUILD_STANDALONE
"  -L             use local context instead of PMCD\n"
#endif
"  -n pmnsfile    use an alternative PMNS\n\
  -x             don't call pmNewContext\n",
                pmProgname);
        exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, 
	       pmnsfile, pmErrStr(sts));
	exit(1);
    }

    if (type >= 0) {
	if (type == 0) {
	    type = PM_CONTEXT_HOST;
	    host = "local:";
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

	if (type == PM_CONTEXT_ARCHIVE) {
	    pmLogLabel	label;
	    if ((sts = pmGetArchiveLabel(&label)) < 0) {
		fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		    pmProgname, pmErrStr(sts));
		exit(1);
	    }
	    if (mode != PM_MODE_INTERP) {
		if ((sts = pmSetMode(mode, &label.ll_start, 0)) < 0) {
		    fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
		    exit(1);
		}
	    }
	}
    }

    /* metricname args are argv[optind] ... argv[argc-1] */
    numpmid = argc - optind;
    namelist = (char **)malloc(numpmid*sizeof(namelist[0]));
    if (namelist == NULL) {
	fprintf(stderr, "%s: namelist malloc(%d) failed\n", pmProgname, (int)(numpmid*sizeof(namelist[0])));
	exit(1);
    }
    pmidlist = (pmID *)malloc(numpmid*sizeof(pmidlist[0]));
    if (pmidlist == NULL) {
	fprintf(stderr, "%s: pmidlist malloc(%d) failed\n", pmProgname, (int)(numpmid*sizeof(pmidlist[0])));
	exit(1);
    }

    for (i = 0; i < numpmid; i++) {
	namelist[i] = argv[optind+i];
	/* bogus pmID to be sure real values are set below pmLookupName() */
	pmidlist[i] = pmid_build(i+1, i+1, i+1);
    }

    sts = pmLookupName(numpmid, namelist, pmidlist);
    printf("pmLookupName -> %d", sts);
    if (sts < 0)
	printf(": %s\n", pmErrStr(sts));
    else
	putchar('\n');
    for (i = 0; i < numpmid; i++) {
	printf("[%d] %s", i, namelist[i]);
	printf(" %s", pmIDStr(pmidlist[i]));
	if (pmidlist[i] == PM_ID_NULL) {
	    /* this one failed */
	    sts = pmLookupName(1, &namelist[i], &pmidlist[i]);
	    printf(" (%s)\n", pmErrStr(sts));
	}
	else {
	    /*
	     * success ... expect reverse lookups to work
	     */
	    char	**names;
	    int		match = 0;
	    sts = pmNameAll(pmidlist[i], &names);
	    if (sts < 0)
		printf(" pmNameAll: %s", pmErrStr(sts));
	    else {
		int	j;
		for (j = 0; j < sts; j++) {
		    if (strcmp(namelist[i], names[j]) == 0) {
			match++;
			break;
		    }
		}
		if (match != 1) {
		    /* oops, no matching name or more than one match! */
		    printf(" botch pmNameAll ->");
		    for (j = 0; j < sts; j++)
			printf(" %s", names[j]);
		}
		free(names);
	    }
	    sts = pmNameID(pmidlist[i], &name);
	    if (sts < 0)
		printf(" pmNameID: %s", pmErrStr(sts));
	    else {
		if (strcmp(namelist[i], name) != 0) {
		    /*
		     * mismatch is OK if dups in PMNS and correct one
		     * returned by pmNameAll
		     */
		    if (match != 1)
			printf(" botch pmNameID -> %s", name);
		}
		free(name);
	    }
	    putchar('\n');
	}
    }

    return 0;
}
