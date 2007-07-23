/*
 * indom_match - exercise indom fetching with a profile
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ident "$Id: torture_indom.c,v 1.1 2002/10/25 01:33:55 kenmcd Exp $"

#include <unistd.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

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
    
    if ((err = pmFetch(1, &pmid, &r1)) < 0) {
	fprintf(stderr, "pmFetch failed for %s: %s\n", name, pmErrStr(err));
	return err;
    }

    if ((err = pmDelProfile(desc.indom, 0, (int *)0)) < 0) {
	fprintf(stderr, "pmDelProfile failed for %s: %s\n", name, pmErrStr(err));
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
    n_r2 = result_instlist(r2, &r2_instlist);

    if (n_r1 != n_r2) {
	fprintf(stderr, "number of instances for unprofiled fetch (%d) != that for the profiled fetch (%d)\n", n_r1, n_r2);
	return PM_ERR_INDOM;
    }

    if (n_r1 != n_instlist) {
	fprintf(stderr, "number of instances from unprofiled fetch (%d) != that for pmGetInDom (%d)\n", n_r1, n_instlist);
	return PM_ERR_INDOM;
    }
    
    if (cmp_list(n_instlist, instlist, r1_instlist) != 0) {
	fprintf(stderr, "instances from pmGetIndom do not match those from unprofiled fetch\n");
	return PM_ERR_INDOM;
    }

    if (cmp_list(n_instlist, instlist, r2_instlist) != 0) {
	fprintf(stderr, "instances from pmGetIndom do not match those from profiled fetch\n");
	return PM_ERR_INDOM;
    }

    if (cmp_list(n_instlist, r1_instlist, r2_instlist) != 0) {
	fprintf(stderr, "instances from unprofiled fetch do not match those from unprofiled fetch\n");
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
    n_r2 = result_instlist(r2, &r2_instlist);

    if (n_r1 != n_r2) {
	fprintf(stderr, "number of instances for first half-profile fetch (%d) != that for the second half-profile fetch (%d)\n", n_r1, n_r2);
	return PM_ERR_INDOM;
    }

    if (n_r1 != n_half_instlist) {
	fprintf(stderr, "number of instances from first half-profile fetch (%d) != that for the half-profile list (%d)\n", n_r1, n_instlist);
	return PM_ERR_INDOM;
    }
    
    if (cmp_list(n_half_instlist, half_instlist, r1_instlist) != 0) {
	fprintf(stderr, "instances from half-profile list do not match those from half-profile fetch\n");
	return PM_ERR_INDOM;
    }

    if (cmp_list(n_half_instlist, half_instlist, r2_instlist) != 0) {
	fprintf(stderr, "instances from half-profile list do not match those from half-profile fetch\n");
	return PM_ERR_INDOM;
    }

    if (cmp_list(n_half_instlist, r1_instlist, r2_instlist) != 0) {
	fprintf(stderr, "instances from first half-profile fetch do not match those from second half-profile fetch\n");
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
    char	*p;
    int		errflag = 0;
    int		type = 0;
    char	*host;
    int		mode = PM_MODE_INTERP;		/* mode for archives */
    pmLogLabel	label;				/* get hostname for archives */
    char	local[MAXHOSTNAMELEN];
    char	*namespace = PM_NS_DEFAULT;
    char	*metricname = (char *)0;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;

    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, "a:D:h:n:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -u allowed\n", pmProgname);
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
		fprintf(stderr, "%s: at most one of -a, -h and -u allowed\n", pmProgname);
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
  -n namespace  use an alternative PMNS\n",
		pmProgname);
	exit(1);
    }

#ifdef REALAPP
    __pmGetLicense(-1, pmProgname, UIMODE);
#endif

    if (namespace != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(namespace)) < 0) {
	    printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	    exit(1);
	}
    }

#ifdef MALLOC_AUDIT
    _malloc_reset_();
    atexit(_malloc_audit_);
#endif

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
	    fprintf(stderr, "%s: Cannot find libirixpmda.so on localhost: %s\n",
		pmProgname, pmErrStr(sts));
	    fprintf(stderr, "\t\t(Check PMDA_PATH environment variable)\n");
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
	if (mode != PM_MODE_INTERP) {
	    if ((sts = pmSetMode(mode, &label.ll_start, 0)) < 0) {
		fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
		exit(1);
	    }
	}
    }

    exit(do_test(metricname));
    /*NOTREACHED*/
}

