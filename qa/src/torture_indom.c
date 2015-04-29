/*
 * indom_match - exercise indom fetching with a profile
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int verbose;

int
result_instlist(pmResult *r, int **list)
{
    int *l;
    int i;

    l = malloc(r->vset[0]->numval * sizeof(int));
    for (i=0; i < r->vset[0]->numval; i++) {
	l[i] = r->vset[0]->vlist[i].inst;
    }
    *list = l;
    return r->vset[0]->numval;
}

int
cmp_list(int n, int *list1, int *list2)
{
    int i;
    int j;
    for (i=0; i < n; i++) {
	for (j=0; j < n; j++) {
	    if (list1[i] == list2[j])
		break;
	}
	if (j == n) {
	    /* fail */
	    return 1;
	}
    }

    /* success */
    return 0;
}

static void
dump_inst(pmInDom indom, char *what, int n, int *list)
{
    int		i;
    int		sts;
    char	*name;

    fprintf(stderr, "%s:\n", what);

    for (i = 0; i < n; i++) {
        if ((sts = pmNameInDom(indom, list[i], &name)) < 0) {
	    fprintf(stderr, "pmNameInDom failed for inst %d: %s\n", list[i], pmErrStr(sts));
	    name = NULL;
        }
	fprintf(stderr, "[%d] %d %s\n", i, list[i], name == NULL ? "???" : name);
	if (name != NULL) free(name);
    }
}


int
do_test(char *name)
{
    pmDesc desc;
    pmID pmid;
    int err;
    char *metriclist[1];
    char **namelist;
    int *instlist;
    int n_instlist;
    pmResult *r1;
    pmResult *r2;
    int n_r1, n_r2;
    int *r1_instlist;
    int *r2_instlist;
    int *half_instlist;
    int n_half_instlist;
    int i;

    metriclist[0] = name;

    if ((err = pmLookupName(1, metriclist, &pmid)) < 0) {
	fprintf(stderr, "pmLookupName failed for %s: %s\n", name, pmErrStr(err));
	return err;
    }

    if ((err = pmLookupDesc(pmid, &desc)) < 0) {
	fprintf(stderr, "pmLookupDesc failed for %s: %s\n", name, pmErrStr(err));
	return err;
    }

    if (desc.indom == PM_INDOM_NULL) {
	fprintf(stderr, "this test only works for non-singular instance domains\n");
	return PM_ERR_INDOM;
    }

    if ((err = pmGetInDom(desc.indom, &instlist, &namelist)) < 0) {
	fprintf(stderr, "pmGetInDom failed for %s: %s\n", name, pmErrStr(err));
	return err;
    }
    else
	n_instlist = err;

    /*
     * check lookup for instances in both directions
     */
    for (i = 0; i < n_instlist; i++) {
	char	*myname;
	if ((err = pmLookupInDom(desc.indom, namelist[i])) < 0) {
	    fprintf(stderr, "pmLookupInDom failed for %s[%s]: %s\n", name, namelist[i], pmErrStr(err));
	    return err;
	}
	if (err != instlist[i]) {
	    fprintf(stderr, "pmLookupInDom %s[%s] -> %d, expecting %d\n", name, namelist[i], err, instlist[i]);
	    return PM_ERR_GENERIC;
	}
	if ((err = pmNameInDom(desc.indom, instlist[i], &myname)) < 0) {
	    fprintf(stderr, "pmNameInDom failed for %s[#%d]: %s\n", name, instlist[i], pmErrStr(err));
	    return err;
	}
	if (strcmp(namelist[i], myname) != 0) {
	    fprintf(stderr, "pmNameInDom %s[#%d] -> %s, expecting %s\n", name, instlist[i], myname, namelist[i]);
	    free(myname);
	    return PM_ERR_GENERIC;
	}
	free(myname);
    }
    
    if ((err = pmFetch(1, &pmid, &r1)) < 0) {
	fprintf(stderr, "pmFetch failed for %s: %s\n", name, pmErrStr(err));
	return err;
    }

    if ((err = pmDelProfile(desc.indom, 0, (int *)0)) < 0) {
	fprintf(stderr, "pmDelProfile failed for %s: %s\n", name, pmErrStr(err));
	return err;
    }
    
    /*
     * DupContext should inherit profile ...
     */
    if ((err = pmDupContext()) < 0) {
	fprintf(stderr, "pmDupContext failed: %s\n", pmErrStr(err));
	return err;
    }

    if ((err = pmAddProfile(desc.indom, n_instlist, instlist)) < 0) {
	fprintf(stderr, "pmAddProfile failed for %s: %s\n", name, pmErrStr(err));
	return err;
    }

    if ((err = pmFetch(1, &pmid, &r2)) < 0) {
	fprintf(stderr, "second pmFetch failed for %s: %s\n", name, pmErrStr(err));
	return err;
    }

    /* now do the checking */
    n_r1 = result_instlist(r1, &r1_instlist);
    if (n_r1 < 0) {
	fprintf(stderr, "unprofiled fetch error: %s\n", pmErrStr(n_r1));
	return n_r1;
    }
    n_r2 = result_instlist(r2, &r2_instlist);
    if (n_r2 < 0) {
	fprintf(stderr, "profiled fetch error: %s\n", pmErrStr(n_r2));
	return n_r2;
    }

    if (n_r1 != n_r2) {
	fprintf(stderr, "number of instances for unprofiled fetch (%d) != that for the profiled fetch (%d)\n", n_r1, n_r2);
	if (verbose) {
	    dump_inst(desc.indom, "unprofiled fetch", n_r1, r1_instlist);
	    dump_inst(desc.indom, "profiled fetch", n_r2, r2_instlist);
	}
	return PM_ERR_INDOM;
    }

    if (n_r1 != n_instlist) {
	fprintf(stderr, "number of instances from unprofiled fetch (%d) != that for pmGetInDom (%d)\n", n_r1, n_instlist);
	if (verbose) {
	    dump_inst(desc.indom, "unprofiled fetch", n_r1, r1_instlist);
	    dump_inst(desc.indom, "pmGetInDom", n_instlist, instlist);
	}
	return PM_ERR_INDOM;
    }
    
    if (cmp_list(n_instlist, instlist, r1_instlist) != 0) {
	fprintf(stderr, "instances from pmGetIndom do not match those from unprofiled fetch\n");
	if (verbose) {
	    dump_inst(desc.indom, "pmGetInDom", n_instlist, instlist);
	    dump_inst(desc.indom, "unprofiled fetch", n_instlist, r1_instlist);
	}
	return PM_ERR_INDOM;
    }

    if (cmp_list(n_instlist, instlist, r2_instlist) != 0) {
	fprintf(stderr, "instances from pmGetIndom do not match those from profiled fetch\n");
	if (verbose) {
	    dump_inst(desc.indom, "pmGetInDom", n_instlist, instlist);
	    dump_inst(desc.indom, "profiled fetch", n_instlist, r2_instlist);
	}
	return PM_ERR_INDOM;
    }

    if (cmp_list(n_instlist, r1_instlist, r2_instlist) != 0) {
	fprintf(stderr, "instances from unprofiled fetch do not match those from profiled fetch\n");
	if (verbose) {
	    dump_inst(desc.indom, "unprofiled fetch", n_instlist, r1_instlist);
	    dump_inst(desc.indom, "profiled fetch", n_instlist, r2_instlist);
	}
	return PM_ERR_INDOM;
    }

    /* delete the whole profile, then add only every second instance */
    if ((err = pmDelProfile(desc.indom, 0, (int *)0)) < 0) {
	fprintf(stderr, "pmDelProfile failed for %s: %s\n", name, pmErrStr(err));
	return err;
    }

    half_instlist = malloc(n_instlist * sizeof(int));
    n_half_instlist = 0;
    for (i=0; i < n_instlist; i+=2) {
	if ((err = pmAddProfile(desc.indom, 1, &instlist[i])) < 0) {
	    fprintf(stderr, "failed to add instance from profile: %s\n", pmErrStr(err));
	    return err;
	}
	half_instlist[n_half_instlist++] = instlist[i];
    }

    pmFreeResult(r1);
    pmFreeResult(r2);
    free(r1_instlist);
    free(r2_instlist);

    if ((err = pmFetch(1, &pmid, &r1)) < 0) {
	fprintf(stderr, "first half-profile pmFetch failed for %s: %s\n", name, pmErrStr(err));
	return err;
    }

    if ((err = pmFetch(1, &pmid, &r2)) < 0) {
	fprintf(stderr, "second half-profile pmFetch failed for %s: %s\n", name, pmErrStr(err));
	return err;
    }

    n_r1 = result_instlist(r1, &r1_instlist);
    if (n_r1 < 0) {
	fprintf(stderr, "first half-profile fetch error: %s\n", pmErrStr(n_r1));
	return n_r1;
    }
    n_r2 = result_instlist(r2, &r2_instlist);
    if (n_r2 < 0) {
	fprintf(stderr, "second half-profile fetch error: %s\n", pmErrStr(n_r2));
	return n_r2;
    }

    if (n_r1 != n_r2) {
	fprintf(stderr, "number of instances for first half-profile fetch (%d) != that for the second half-profile fetch (%d)\n", n_r1, n_r2);
	if (verbose) {
	    dump_inst(desc.indom, "first half-profile fetch", n_r1, r1_instlist);
	    dump_inst(desc.indom, "second half-profile fetch", n_r2, r2_instlist);
	}
	return PM_ERR_INDOM;
    }

    if (n_r1 != n_half_instlist) {
	fprintf(stderr, "number of instances from first half-profile fetch (%d) != that for the half-profile list (%d)\n", n_r1, n_instlist);
	if (verbose) {
	    dump_inst(desc.indom, "first half-profile fetch", n_r1, r1_instlist);
	    dump_inst(desc.indom, "half-profile list", n_instlist, instlist);
	}
	return PM_ERR_INDOM;
    }
    
    if (cmp_list(n_half_instlist, half_instlist, r1_instlist) != 0) {
	fprintf(stderr, "instances from half-profile list do not match those from first half-profile fetch\n");
	if (verbose) {
	    dump_inst(desc.indom, "half-profile list", n_half_instlist, half_instlist);
	    dump_inst(desc.indom, "first half-profile fetch", n_half_instlist, r1_instlist);
	}
	return PM_ERR_INDOM;
    }

    if (cmp_list(n_half_instlist, half_instlist, r2_instlist) != 0) {
	fprintf(stderr, "instances from half-profile list do not match those from second half-profile fetch\n");
	if (verbose) {
	    dump_inst(desc.indom, "half-profile list", n_half_instlist, half_instlist);
	    dump_inst(desc.indom, "second half-profile fetch", n_half_instlist, r2_instlist);
	}
	return PM_ERR_INDOM;
    }

    if (cmp_list(n_half_instlist, r1_instlist, r2_instlist) != 0) {
	fprintf(stderr, "instances from first half-profile fetch do not match those from second half-profile fetch\n");
	if (verbose) {
	    dump_inst(desc.indom, "first half-profile fetch", n_half_instlist, r1_instlist);
	    dump_inst(desc.indom, "second half-profile fetch", n_half_instlist, r2_instlist);
	}
	return PM_ERR_INDOM;
    }

    /* success */
    return 0;
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*errmsg;
    int		type = 0;
    char	*host = NULL;			/* pander to gcc */
    pmLogLabel	label;				/* get hostname for archives */
    char	local[MAXHOSTNAMELEN];
    char	*namespace = PM_NS_DEFAULT;
    char	*metricname = (char *)0;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:K:Ln:v?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
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
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'K':	/* update local PMDA table */
	    if ((errmsg = __pmSpecLocalPMDA(optarg)) != NULL) {
		fprintf(stderr, "%s: __pmSpecLocalPMDA failed: %s\n", pmProgname, errmsg);
		errflag++;
	    }
	    break;

	case 'L':	/* local PMDA connection, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_LOCAL;
	    gethostname(local, sizeof(local));
	    host = local;
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 'v':	/* verbose */
	    verbose++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */

    if (optind == argc-1) {
	metricname = argv[optind];
	optind++;
    }
    else {
	fprintf(stderr, "metricname is a required argument\n");
	errflag++;
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] metricname\n\
\n\
Options\n\
  -a archive	metrics source is an archive log\n\
  -D debug	standard PCP debug flag\n\
  -h host	metrics source is PMCD on host (default is local libirixpmda)\n\
  -L            metrics source is local connection to PMDA, no PMCD\n\
  -K spec       optional additional PMDA spec for local connection\n\
                spec is of the form op,domain,dso-path,init-routine\n\
  -n namespace  use an alternative PMNS\n\
  -v            verbose\n",
		pmProgname);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT) {
	if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	    printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	    exit(1);
	}
    }

    if (type == 0) {
	type = PM_CONTEXT_LOCAL;
	gethostname(local, sizeof(local));
	host = local;
    }
    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	else if (type == PM_CONTEXT_LOCAL) {
	    fprintf(stderr, "%s: pmNewContext failed for PM_CONTEXT_LOCAL: %s\n",
		pmProgname, pmErrStr(sts));
	}
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }

    exit(do_test(metricname));
}

