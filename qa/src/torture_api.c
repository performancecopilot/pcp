/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * main - general purpose exerciser of much of the PMAPI
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

static int	_op;  /* number of api operations */
static int	_err; /* number of api errors */
static int	vflag;
static char	*context_name = "localhost";
static int 	context_type; /* archive, host or local */
static char	*namespace = PM_NS_DEFAULT;
static int	all_children = 1; /* do the children of "" test */
static int	root_children; /* only do the children of "" test */
static int	dump_metrics; /* just dump the metrics and exit */
/*
 * pmns_style == 1 => do PMNS style loading the old way
 * pmns_style == 2 => try to use the distributed PMNS
 */
static int pmns_style = 1;

/* The list of metrics to test out */
static char *namelist[] = {
    "disk.all.total",
    "pmcd",
    "kernel.all.pswitch",
    "kernel.all.cpu.user",
    "kernel.all.cpu.wait.total",
    "hinv.ncpu",
    "pmcd.control",
    "sampledso.aggregate.hullo",
    "sample.seconds",
    "sample.colour",
    "sample.longlong",
    "bozo.the.clown"
};

#define MAXNAMES (sizeof(namelist)/sizeof(char*))


typedef struct name_status {
  char *name;
  int status;
}name_status;

static int
compar_str(const void *a, const void *b)
{
    char	**ca = (char **)a;
    char	**cb = (char **)b;
    return strcmp(*ca, *cb);
}

static int
compar_name_status(const void *a, const void *b)
{
    name_status	*ca = (name_status*)a;
    name_status	*cb = (name_status*)b;
    return strcmp(ca->name, cb->name);
}

void
do_chn(char *name)
{
    int		n;
    int		j;
    char	**enfants = NULL;
    name_status *ns_table = NULL;
    int 	has_children = 0;

    _op++;
    n = pmGetChildren(name, &enfants);
    if (n < 0) {
	_err++;
	printf("pmGetChildren: %s\n", pmErrStr(n));
    }
    else if (n > 0) {
	qsort(enfants, n, sizeof(enfants[0]), compar_str);
	has_children = 1;
    }

    /* test out pmGetChildrenStatus */
    {
        char	**s_enfants = NULL;
        int	*status = NULL;

        _op++;
        n = pmGetChildrenStatus(name, &s_enfants, &status);
	if (n < 0) {
	    _err++;
	    printf("pmGetChildrenStatus: %s\n", pmErrStr(n));
	}
	else if (n > 0) {
	    /* create a ns_table for sorting */
            ns_table = (name_status*)malloc(sizeof(name_status)*n);
	    if (ns_table == 0) {
	       printf("Malloc failed\n");
	       exit(1);
	    }
	    for(j = 0; j < n; j++) { 
	      ns_table[j].name = s_enfants[j];
	      ns_table[j].status = status[j];
	    }

	    qsort(ns_table, n, sizeof(ns_table[0]), compar_name_status);

	    for (j = 0; j < n; j++) {
	      if (strcmp(ns_table[j].name, enfants[j]) != 0) {
	         printf("pmGetChildrenStatus mismatch: \"%s\" vs \"%s\"\n",
		        enfants[j], ns_table[j].name);
              }
	    }/*for*/
	}/*if*/
	if (s_enfants) free(s_enfants);
        if (status) free(status);
    }

    if (has_children && vflag) {
	printf("children of \"%s\" ...\n", name);
	for (j = 0; j < n; j++) {

	    printf("    %-20s", enfants[j]);
	    if (ns_table)
	      printf("<s = %d>", ns_table[j].status);
	    printf("\n");
	}
    }

    if (enfants) free(enfants);
    if (ns_table) free(ns_table);

}

void
parse_args(int argc, char **argv)
{
    extern char	*optarg;
    extern int	optind;
    int		errflag = 0;
    int		c;
    static char	*usage = "[-bcLmv] [-a archive] [-h host] [-n namespace] [-s 1|2]";
    char	*endnum;
    int		sts;
#ifdef PCP_DEBUG
    static char	*debug = "[-D <dbg>]";
#else
    static char	*debug = "";
#endif

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:iLmn:s:vbc")) != EOF) {
	switch (c) {
	case 'a':	/* archive name for context */
            if (context_type != 0) {
	        fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n",
                        pmProgname);
		errflag++;
	    }
	    context_type = PM_CONTEXT_ARCHIVE;
	    context_name = optarg;
	    break;

	case 'b':	/* dont do the children of "" test */
	    all_children = 0;
	    break;

	case 'c':	/* only do the children of "" test */
	    root_children = 1;
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

	case 'h':	/* context_namename for live context */
            if (context_type != 0) {
	        fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n",
                        pmProgname);
		errflag++;
	    }
	    context_type = PM_CONTEXT_HOST;
	    context_name = optarg;
	    break;

	case 'i':	/* non-IRIX names (always true now) */
	    break;

	case 'L':	/* LOCAL context */
            if (context_type != 0) {
	        fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n",
                        pmProgname);
		errflag++;
	    }
	    context_type = PM_CONTEXT_LOCAL;
	    context_name = NULL;
	    break;

        case 'm':       /* dump out the list of metrics to be tested */
	    dump_metrics = 1;
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 'v':	/* verbose */
	    vflag++;
	    break;

	case 's':	/* pmns style */
	    pmns_style = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		printf("%s: -s requires numeric argument\n", pmProgname);
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
	printf("Usage: %s %s%s\n", pmProgname, debug, usage);
	exit(1);
    }
}

void
load_namespace(char *namespace)
{
    struct timeval	now, then;
    int sts;

    gettimeofday(&then, (struct timezone *)0);
    _op++;
    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	_err++;
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }
    gettimeofday(&now, (struct timezone *)0);
    printf("Name space load: %.2f msec\n", __pmtimevalSub(&now, &then)*1000);
}

void 
test_api(void)
{
    int			sts;
    int			i;
    pmID		midlist[MAXNAMES];
    int			*instlist;
    char		**inamelist;
    int			n;
    int			numpmid = MAXNAMES;
    char		**names;
    pmResult		*resp;
    pmDesc		desc;

    _op++;
    
    if (context_type == 0) {
	char local[MAXHOSTNAMELEN];
	context_type = PM_CONTEXT_HOST;
	gethostname(local, sizeof(local));
	context_name = local;
    }

    if ((sts = pmNewContext(context_type, context_name)) < 0) {
	_err++;
	printf("%s: Error in creating %s context for \"%s\": %s\n", 
	       pmProgname, 
	       context_type == PM_CONTEXT_HOST ? "host" :
	       context_type == PM_CONTEXT_ARCHIVE ? "archive" :
	       "local", 
	       context_name,
	       pmErrStr(sts));
    }

    if (vflag > 1) {
	_op++;
	__pmDumpNameSpace(stdout, 1);
    }


    _op++;
    n = pmLookupName(numpmid, namelist, midlist);
    if (n < 0 && n != PM_ERR_NONLEAF) {
	/*
	 * PM_ERR_NONLEAF would be from an older pmcd/libpcp, before the
	 * pmLookupName() return value fix up.
	 */
	_err++;
	printf("pmLookupName: Unexpected error: %s\n", pmErrStr(n));
	for (i = 0; i < numpmid; i++) {
	    printf("name[%d] %s -> %s\n", i, namelist[i], pmIDStr(midlist[i]));
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
      for (i = 0; i < numpmid; i++) {
        printf("%s: id[%d] = %d\n", namelist[i], i, midlist[i]);
      }
    }
#endif

    /* Set mode for archive so that an indom can be retrieved 
     * if necessary.
     * Getting an indom is done based on current time.
     * If we try and get an indom from the start then we may not
     * have one yet.
     */
    if (context_type == PM_CONTEXT_ARCHIVE) {
	struct timeval	when;

	_op++;
	if ((n = pmGetArchiveEnd(&when)) < 0) {
	    _err++;
	    printf("pmGetArchiveEnd: %s\n", pmErrStr(n));
	}

	_op++;
	if ((n = pmSetMode(PM_MODE_BACK, &when, 1000)) < 0) {
	    _err++;
	    printf("pmSetMode(PM_MODE_BACK): %s\n", pmErrStr(n));
	}
    }

    for (i = 0; i < numpmid && !root_children; i++) {
	putchar('\n');
	if (vflag) printf("pmid: %s ", pmIDStr(midlist[i]));
	printf("name: %s\n", namelist[i]);
	if (strcmp(namelist[i], "bozo.the.clown") != 0)
	    do_chn(namelist[i]);
	if (midlist[i] != PM_ID_NULL) {
	    _op++;
	    n = pmNameAll(midlist[i], &names);
	    if (n < 0) {
		_err++;
		printf("pmNameAll: %s\n", pmErrStr(n));
	    }
	    else {
		/*
		 * strange [5] here is to skip over kernel. prefix ...
		 * this is from a bog of eternal stench, long, long ago
		 */
		int	k;
		for (k = 0; k < n; k++) {
		    if (strcmp(namelist[i], names[k]) == 0 ||
			strcmp(&namelist[i][5], names[k]) == 0)
			break;
		}
		if (k == n) {
		    _err++;
		    printf("pmNameAll botch: expected \"%s\", got \"",
			namelist[i]);
		    __pmPrintMetricNames(stdout, n, names, " or ");
		    printf("\"\n");
		}
		free(names);
	    }
	    _op++;
	    if ((n = pmLookupDesc(midlist[i], &desc)) < 0) {
		_err++;
		printf("pmLookupDesc: %s\n", pmErrStr(n));
	    }
	    else {
		if (vflag > 1) {
		    const char	*u = pmUnitsStr(&desc.units);
		    printf("desc: type=%d indom=0x%x sem=%d units=%s\n",
			desc.type, desc.indom, desc.sem,
			*u == '\0' ? "none" : u);
		}
		if (desc.indom == PM_INDOM_NULL)
		    continue;
		_op++;
		if ((n = pmGetInDom(desc.indom, &instlist, &inamelist)) < 0) {
		    _err++;
		    printf("pmGetInDom: %s\n", pmErrStr(n));
		}
		else {
		    int		j;
		    int		numinst = n;
		    char	*name;
		    for (j = 0; j < numinst; j++) {
			if (vflag > 1)
			    printf("  instance id: 0x%x\n", instlist[j]);
			_op++;
			if ((n = pmNameInDom(desc.indom, instlist[j], &name)) < 0) {
			    _err++;
			    printf("pmNameInDom: %s\n", pmErrStr(n));
			}
			else {
			    if (vflag > 1)
				printf(" %s (== %s?)\n", name, inamelist[j]);
			    _op++;
			    if ((n = pmLookupInDom(desc.indom, name)) < 0) {
				_err++;
				printf("pmLookupInDom: %s\n", pmErrStr(n));
			    }
			    else {
				if (n != instlist[j]) {
				    _err++;
				    printf("botch: pmLookupInDom returns 0x%x, expected 0x%x\n",
					n, instlist[j]);
				}
			    }
			    free(name);
			}
		    }
		    free(instlist);
		    free(inamelist);
		}
	    }
	}
    }/*for each named metric*/ 

    if (all_children || root_children) {
	/* root check */
	/* 
	 * This is only useful in a namspace where irix has not been stripped from the
	 * names. Once irix is stripped from the names, the list returned becomes
	 * to variable to do a QA in this manner.
	 * Test #568 using pmns_xlate tests the root.
	 */
	printf("\n");
	do_chn("");
    }

    if (context_type == PM_CONTEXT_ARCHIVE) {
	struct timeval	when;

        when.tv_sec = 0;
	when.tv_usec = 0;

	if (vflag) 
	    printf("\nArchive result ...\n");
        for (i = 0; i < numpmid; i++) {
	    if (midlist[i] == PM_ID_NULL)
		continue; 
	    _op++;
	    if ((n = pmSetMode(PM_MODE_FORW, &when, 0)) < 0) {
		_err++;
		printf("pmSetMode(PM_MODE_FORW): %s\n", pmErrStr(n));
	    }
	    else {
		_op++;
                if (vflag)
                    printf("Fetch of %s:\n", namelist[i]);
		if ((n = pmFetch(1, &midlist[i], &resp)) < 0) {
		    _err++;
		    printf("Archive pmFetch: %s\n", pmErrStr(n));
		}
		else {
		    if (vflag)
			__pmDumpResult(stdout, resp);
		    _op++;
		    pmFreeResult(resp);
		}
	    }
  	}/*for*/
    }

    else if (context_type == PM_CONTEXT_HOST) {
	_op++;
	if ((n = pmSetMode(PM_MODE_LIVE, (struct timeval *)0, 0)) < 0) {
	    _err++;
	    printf("pmSetMode(PM_MODE_LIVE): %s\n", pmErrStr(n));
	}
	else {
	    _op++;
	    if ((n = pmFetch(numpmid, midlist, &resp)) < 0) {
		_err++;
		printf("real-time pmFetch: %s\n", pmErrStr(n));
	    }
	    else {
		if (vflag) {
		    printf("\nReal-time result ...\n");
		    __pmDumpResult(stdout, resp);
		}
		_op++;
		pmFreeResult(resp);
	    }
	}
    }

}

int
main(int argc, char **argv)
{
  parse_args(argc, argv);

    if (dump_metrics == 1) {
	int i;
	for(i = 0; i < MAXNAMES; i++) {
	    printf("%s\n", namelist[i]);
	}
	exit(0);
    }

  if (pmns_style == 2) {
    /* test it the new way with distributed namespace */
    /* i.e. no client loaded namespace */
    test_api();
  }
  else {
    /* test it the old way with namespace file */
    load_namespace(namespace);
    test_api();
  }

  printf("\nUnexpected failure for %d of %d PMAPI operations\n", _err, _op);
  exit(0);
}
