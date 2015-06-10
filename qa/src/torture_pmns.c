/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Ken McDonell.  All Rights Reserved.
 *
 * Exercise just the PMNS functions ... intended for dynamic PMNS testing
 * ... this is really torture_api re-tweaked
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

static int	vflag;
static char	*context_name = "localhost";
static int 	context_type = 0; /* archive, host or local */
static char	*namespace = PM_NS_DEFAULT;
static int	all_children = 1; /* do the children of "" test */
static int	root_children; /* only do the children of "" test */
static int	dump_metrics; /* just dump the metrics and exit */
/*
 * pmns_style == 1 => do PMNS style loading the old way
 * pmns_style == 2 => try to use the distributed PMNS
 */
static int pmns_style = 2;

static int	numpmid = 0;
static char 	**namelist;	/* The list of metrics to test out */
static pmID	*midlist;

typedef struct name_status {
  char *name;
  int status;
}name_status;

#define REPORT(str, sts) \
printf("%s() returns %d", str, sts);\
if (sts < 0) printf(" (%s)", pmErrStr(sts));\
putchar('\n');

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

    n = pmGetChildren(name, &enfants);
    REPORT("pmGetChildren", n);
    if (n > 0) {
	qsort(enfants, n, sizeof(enfants[0]), compar_str);
	has_children = 1;
    }

    /* test out pmGetChildrenStatus */
    {
        char	**s_enfants = NULL;
        int	*status = NULL;

        n = pmGetChildrenStatus(name, &s_enfants, &status);
	REPORT("pmGetChildrenStatus", n);
	if (n > 0) {
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
    static char	*usage = "[-bcLmvx] [-a archive] [-h host] [-n namespace] [-s 1|2] metricname ...";
    char	*endnum;
    int		sts;
#ifdef PCP_DEBUG
    static char	*debug = "[-D <dbg>]";
#else
    static char	*debug = "";
#endif

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:bcD:h:iLmn:s:vx")) != EOF) {
	switch (c) {
	case 'a':	/* archive name for context */
            if (context_type != 0) {
	        fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n",
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
	        fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n",
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
	        fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n",
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

	case 'x':	/* NO context */
            if (context_type != 0) {
	        fprintf(stderr, "%s: at most one of -a, -h, -L and -x allowed\n",
                        pmProgname);
		errflag++;
	    }
	    context_type = -1;
	    context_name = NULL;
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

    if (context_type == 0) context_type = PM_CONTEXT_HOST;

    numpmid = argc - optind;
    if (numpmid < 1) {
	errflag++;
    }
    else {
	int	i;
	if ((midlist = (pmID *)malloc(numpmid*sizeof(pmID))) == NULL) {
	    fprintf(stderr, "malloc failed for midlist[]: %s\n", strerror(errno));
	    exit(1);
	}
	if ((namelist = (char **)malloc(numpmid*sizeof(char *))) == NULL) {
	    fprintf(stderr, "malloc failed for namelist[]: %s\n", strerror(errno));
	    exit(1);
	}
	for (i = 0; i < numpmid; i++)
	    namelist[i] = argv[optind+i];
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
    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
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
    int			*instlist;
    char		**inamelist;
    char		**allnames;
    int			n;
    char		*back;
    pmResult		*resp;
    pmDesc		desc;
    
    if (context_type != -1) {
	if (context_type == 0) {
	    char local[MAXHOSTNAMELEN];
	    context_type = PM_CONTEXT_HOST;
	    gethostname(local, sizeof(local));
	    context_name = local;
	}

	if ((sts = pmNewContext(context_type, context_name)) < 0) {
	    printf("%s: Error in creating %s context for \"%s\": %s\n", 
		   pmProgname, 
		   context_type == PM_CONTEXT_HOST ? "host" :
		   context_type == PM_CONTEXT_ARCHIVE ? "archive" :
		   "local", 
		   context_name,
		   pmErrStr(sts));
	}
    }

    if (vflag > 1) {
	__pmDumpNameSpace(stdout, 1);
    }

    n = pmLookupName(numpmid, namelist, midlist);
    REPORT("pmLookupName", n);

    for (i = 0; i < numpmid; i++) {
        printf("%s: id[%d] = %s\n", namelist[i], i, pmIDStr(midlist[i]));
    }

    /* Set mode for archive so that an indom can be retrieved 
     * if necessary.
     * Getting an indom is done based on current time.
     * If we try and get an indom from the start then we may not
     * have one yet.
     */
    if (context_type == PM_CONTEXT_ARCHIVE) {
	struct timeval	when;

	n = pmGetArchiveEnd(&when);
	REPORT("pmGetArchiveEnd", n);

	n = pmSetMode(PM_MODE_BACK, &when, 1000);
	REPORT("pmSetMode", n);
    }

    for (i = 0; i < numpmid && !root_children; i++) {
	putchar('\n');
	if (vflag) {
	    printf("=== metric %d === name: %s pmid %s\n", i, namelist[i], pmIDStr(midlist[i]));
	}
	if (midlist[i] != PM_ID_NULL) {
	    n = pmNameID(midlist[i], &back);
	    REPORT("pmNameID", n);
	    if (n >= 0) {
		if (vflag) {
		    printf("pmid: %s ", pmIDStr(midlist[i]));
		    printf(" name: %s\n", back);
		}
		if (strcmp(namelist[i], back) != 0) {
		    printf("pmNameID botch: expected \"%s\", got \"%s\"\n",
			namelist[i], back);
		}
		free(back);
	    }
	    n = pmNameAll(midlist[i], &allnames);
	    REPORT("pmNameAll", n);
	    if (n >= 0) {
		int		j;
		for (j = 0; j < n; j++) {
		    if (vflag) {
			printf("pmid: %s ", pmIDStr(midlist[i]));
			printf(" name: %s\n", allnames[j]);
		    }
		    if (strcmp(namelist[i], allnames[j]) != 0) {
			printf("pmNameAll info: expected \"%s\", got \"%s\"\n",
			    namelist[i], allnames[j]);
		    }
		}
		free(allnames);
	    }
	    if (context_type != -1) {
		n = pmLookupDesc(midlist[i], &desc);
		REPORT("pmLookupDesc", n);
		if (n >= 0) {
		    if (vflag > 1) {
			const char	*u = pmUnitsStr(&desc.units);
			printf("desc: type=%d indom=0x%x sem=%d units=%s\n",
			    desc.type, desc.indom, desc.sem,
			    *u == '\0' ? "none" : u);
		    }
		    if (desc.indom == PM_INDOM_NULL)
			continue;
		    n = pmGetInDom(desc.indom, &instlist, &inamelist);
		    REPORT("pmGetInDom", n);
		    if (n >= 0) {
			int		j;
			int		numinst = n;
			char	*name;
			for (j = 0; j < numinst; j++) {
			    if (vflag > 1)
				printf("  instance id: 0x%x\n", instlist[j]);
			    n = pmNameInDom(desc.indom, instlist[j], &name);
			    REPORT("pmNameInDom", n);
			    if (n >= 0) {
				if (vflag > 1)
				    printf(" %s (== %s?)\n", name, inamelist[j]);
				n = pmLookupInDom(desc.indom, name);
				REPORT("pmLookupInDom", n);
				if (n >= 0) {
				    if (n != instlist[j]) {
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
	}
	do_chn(namelist[i]);
    } /* for each named metric */ 

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
	    if ((n = pmSetMode(PM_MODE_FORW, &when, 0)) < 0) {
		printf("pmSetMode(PM_MODE_FORW): %s\n", pmErrStr(n));
	    }
	    else {
                if (vflag)
                    printf("Fetch of %s:\n", namelist[i]);
		if ((n = pmFetch(1, &midlist[i], &resp)) < 0) {
		    printf("Archive pmFetch: %s\n", pmErrStr(n));
		}
		else {
		    if (vflag)
			__pmDumpResult(stdout, resp);
		    pmFreeResult(resp);
		}
	    }
  	}/*for*/
    }

    else if (context_type == PM_CONTEXT_HOST) {
	if ((n = pmSetMode(PM_MODE_LIVE, (struct timeval *)0, 0)) < 0) {
	    printf("pmSetMode(PM_MODE_LIVE): %s\n", pmErrStr(n));
	}
	else {
	    if ((n = pmFetch(numpmid, midlist, &resp)) < 0) {
		printf("real-time pmFetch: %s\n", pmErrStr(n));
	    }
	    else {
		if (vflag) {
		    printf("\nReal-time result ...\n");
		    __pmDumpResult(stdout, resp);
		}
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
	for(i = 0; i < numpmid; i++) {
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

  exit(0);
}
