/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * permfetch - fetch, permute and fetch again - for a bunch of metrics
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

typedef struct {
    char	*base;
    int		numpmid;
    char	**namelist;
    pmID	*pmidlist;
    pmResult	*rp;
} state_t;

state_t		*todolist;
static int	todo;

static void
dometric(const char *name)
{
    int		done = todolist[todo].numpmid;

    todolist[todo].namelist = (char **)realloc(todolist[todo].namelist, (1+done)*sizeof(char *));
    todolist[todo].pmidlist = (pmID *)realloc(todolist[todo].pmidlist, (1+done)*sizeof(pmID));
    todolist[todo].namelist[done] = strdup(name);
    todolist[todo].numpmid++;
}

int
main(int argc, char **argv)
{
    int		c;
    int		n;
    int		j;
    int		numpmid;
    pmID	pmid;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    int 	verbose = 0;
    char	*host = NULL;			/* pander to gcc */
    char	local[MAXHOSTNAMELEN];
    char	*namespace = PM_NS_DEFAULT;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:n:V?")) != EOF) {
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

	case 'V':	/* verbose */
	    verbose++;
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
  -a   archive	  metrics source is an archive log\n\
  -D   debug	  standard PCP debug flag\n\
  -h   host	  metrics source is PMCD on host\n\
  -n   namespace  use an alternative PMNS\n\
  -V 	          verbose/diagnostic output\n",
		pmProgname);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
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

    /* non-flag args are argv[optind] ... argv[argc-1] */
    while (optind < argc) {
	todolist = (state_t *)realloc(todolist, (todo+1)*sizeof(state_t));
	todolist[todo].base = argv[optind];
	todolist[todo].numpmid = 0;
	todolist[todo].namelist = (char **)0;
	todolist[todo].pmidlist = (pmID *)0;
	pmTraversePMNS(argv[optind], dometric);
	printf("%s:\n", argv[optind]);
	if (verbose)
	    printf("... %d metrics,", todolist[todo].numpmid);
	if (todolist[todo].numpmid == 0) {
	    printf("... no metrics in PMNS ... skip tests\n");
	    goto next;
	}
	sts = pmLookupName(todolist[todo].numpmid, todolist[todo].namelist, todolist[todo].pmidlist);
	if (sts != todolist[todo].numpmid) {
	    int		i;
	    putchar('\n');
	    if (sts < 0)
		fprintf(stderr, "%s: pmLookupName: %s\n", pmProgname, pmErrStr(sts));
	    else
		fprintf(stderr, "%s: pmLookupName: returned %d, expected %d\n", pmProgname, sts, todolist[todo].numpmid);
	    for (i = 0; i < todolist[todo].numpmid; i++) {
		if (todolist[todo].pmidlist[i] == PM_ID_NULL)
		    fprintf(stderr, "   %s is bad\n", todolist[todo].namelist[i]);
	    }
	    exit(1);
	}
	sts = pmFetch(todolist[todo].numpmid, todolist[todo].pmidlist, &todolist[todo].rp);
	if (sts < 0) {
	    putchar('\n');
	    fprintf(stderr, "%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
	if (verbose)
	    printf(" %d value sets\n", todolist[todo].rp->numpmid);
	if (todolist[todo].numpmid != todolist[todo].rp->numpmid)
	    printf("botch: %d metrics != %d value sets!\n",
		todolist[todo].numpmid, todolist[todo].rp->numpmid);

next:
	optind++;
	todo++;
    }

    for (n = 0; n < todo; n++) {
	if (todolist[todo-n-1].numpmid <= 0)
	    continue;
	printf("%s: free names and result\n", todolist[todo-n-1].base);
	for (j = 0; j < todolist[todo-n-1].numpmid; j++) {
	    free(todolist[todo-n-1].namelist[j]);
	}
	free(todolist[todo-n-1].namelist);
	pmFreeResult(todolist[todo-n-1].rp);
    }
    /* reverse pmids in list */
    for (n = 0; n < todo; n++) {
	numpmid = todolist[n].numpmid;
	for (j = 0; j < numpmid / 2; j++) {
	    pmid = todolist[n].pmidlist[numpmid-j-1];
	    todolist[n].pmidlist[numpmid-j-1] = todolist[n].pmidlist[j];
	    todolist[n].pmidlist[j] = pmid;
	}
    }
    for (n = 0; n < todo; n++) {
	if (todolist[n].numpmid <= 0)
	    continue;
	printf("%s (reverse):\n", todolist[n].base);
	if (verbose)
	    printf("... %d metrics,", todolist[n].numpmid);
	sts = pmFetch(todolist[n].numpmid, todolist[n].pmidlist, &todolist[n].rp);
	if (sts < 0) {
	    putchar('\n');
	    fprintf(stderr, "%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
	if (todolist[n].numpmid != todolist[n].rp->numpmid)
	    printf("botch: %d metrics != %d value sets!\n",
		todolist[n].numpmid, todolist[n].rp->numpmid);

	if (verbose) {
	    printf(" %d value sets", todolist[n].rp->numpmid);
	    printf(" free result\n");
	}
	pmFreeResult(todolist[n].rp);
    }

    exit(0);
}
