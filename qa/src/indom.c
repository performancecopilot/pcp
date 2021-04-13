/*
 * indom - exercise pmGetInDom, pmNameInDom and pmLookupInDom
 */

#include <pcp/pmapi.h>

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_ARCHIVE,
    PMOPT_DEBUG,
    PMOPT_HOST,
    { "full", 0, 'f', NULL, "report all instances" },
    { "inst", 1, 'i', "INST", "report this internal instance identifier" },
    { "probe", 1, 'r', "PCT", "report PCT percentile internal instances" },
    { "values", 0, 'v', NULL, "report values as well" },
    PMOPT_LOCALPMDA,
    PMOPT_SPECLOCAL,
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ,
    .short_options = "a:D:fh:i:K:Lr:vzZ:?",
    .long_options = longopts,
    .short_usage = "[options] [-f|-p|-i INST] metricname ...",
};

static int	inst = -999;

static int	rflag = 0;
static int	pct;
static int	fflag = 0;
static int	vflag = 0;

int
dometric(const char *name)
{
    pmID	pmid;
    pmDesc	desc;
    int		i, sts;
    int		numinst;
    char	*iname = NULL;
    int		*ilist = NULL;
    int		p;
    int		probe;
    int		nprobe;
    char	**nlist;
    char	*func;

    if ((sts = pmLookupName(1, &name, &pmid)) < 0)
	return sts;

    if ((sts = pmLookupDesc(pmid, &desc)) < 0)
	return sts;

    if (opts.context == PM_CONTEXT_HOST || opts.context == PM_CONTEXT_LOCAL) {
	numinst = pmGetInDom(desc.indom, &ilist, &nlist);
	func = "pmGetInDom";
    }
    else {
	numinst = pmGetInDomArchive(desc.indom, &ilist, &nlist);
	func = "pmGetInDomArchive";
    }
    if (rflag || fflag) {
	if (numinst > 0)
	    nprobe = numinst;
	else
	    nprobe = -1;
    }
    else
	nprobe = 1;


    for (p = 0; p < nprobe; p++) {
	int	try;
	iname = "no match";
	if (rflag) {
	    probe = p*pct*(numinst-1)/100;
	    if (probe >= numinst)
		break;
	    try = ilist[probe];
	}
	else if (fflag)
	    try = ilist[p];
	else
	    try = inst;
	printf("pm*InDom: inst=[%d]", try);
	if ((sts = pmNameInDom(desc.indom, try, &iname)) < 0)
	    printf(" {%s}\n", pmErrStr(sts));
	else {
	    printf(" iname=<%s> reverse lookup:", iname);
	    if ((sts = pmLookupInDom(desc.indom, iname)) < 0)
		printf(" {%s}", pmErrStr(sts));
	    else
		printf(" inst=[%d]", sts);
	    free(iname);
	    if (vflag) {
		if ((sts = pmDelProfile(desc.indom, 0, NULL)) < 0)
		    printf(" pmDelProfile: {%s}", pmErrStr(sts));
		else {
		    if ((sts = pmAddProfile(desc.indom, 1, &try)) < 0)
			printf(" pmAddProfile: {%s}", pmErrStr(sts));
		    else {
			pmResult	*rp;
			if ((sts = pmFetch(1, &desc.pmid, &rp)) < 0)
			    printf(" pmFetch: {%s}", pmErrStr(sts));
			else {
			    printf(" value:");
			    pmPrintValue(stdout, rp->vset[0]->valfmt, desc.type, &rp->vset[0]->vlist[0], 9);
			    pmFreeResult(rp);
			}
		    }
		}
	    }
	    fputc('\n', stdout);
	}
	if (opts.context == PM_CONTEXT_ARCHIVE) {
	    iname = "no match";
	    printf("pm*InDomArchive: inst=[%d]", try);
	    if ((sts = pmNameInDomArchive(desc.indom, try, &iname)) < 0)
		printf(" {%s}\n", pmErrStr(sts));
	    else {
		printf(" iname=<%s> reverse lookup:", iname);
		if ((sts = pmLookupInDomArchive(desc.indom, iname)) < 0)
		    printf(" {%s}\n", pmErrStr(sts));
		else
		    printf(" inst=[%d]\n", sts);
		free(iname);
	    }
	}
    }

    if (numinst < 0) {
	printf("%s: {%s}\n", func, pmErrStr(numinst));
	sts = numinst;
    }
    else {
	sts = 0;
	printf("%s:\n", func);
	if (rflag) {
	    for (p = 0; p < nprobe; p++) {
		probe = p*pct*(numinst-1)/100;
		if (probe >= numinst)
		    break;
		printf("   [%d] <%s> [%d%% percentile]\n", ilist[probe], nlist[probe], p*pct);
	    }
	}
	else if (fflag) {
	    for (p = 0; p < nprobe; p++) {
		printf("   [%d] <%s> [ordinal %d]\n", ilist[p], nlist[p], p);
	    }
	}
	else {
	    for (i = 0; i < numinst; i++) {
		if (ilist[i] == inst) {
		    printf("   [%d] <%s>\n", ilist[i], nlist[i]);
		    break;
		}
	    }
	}
	free(ilist);
	free(nlist);
    }

    return sts;
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		exitsts = 0;
    int		contextid;
    char	*source;
    char	*endnum;

    pmSetProgname(argv[0]);
    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'f':	/* pick all instances */
	    if (rflag) {
		pmprintf("%s: -p and -f are mutually exclusive\n", pmGetProgname());
		opts.errors++;
	    }
	    fflag++;
	    break;
	case 'i':	/* instance */
	    inst = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || inst < 0) {
		pmprintf("%s: -i requires numeric argument\n", pmGetProgname());
		opts.errors++;
	    }
	    break;
	case 'r':	/* pick some representative instances */
	    if (fflag) {
		pmprintf("%s: -f and -p are mutually exclusive\n", pmGetProgname());
		opts.errors++;
	    }
	    rflag++;
	    pct = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || pct <= 0 || pct > 99) {
		pmprintf("%s: -p requires numeric argument > 0 and <= 99\n", pmGetProgname());
		opts.errors++;
	    }
	    break;
	case 'v':	/* report values as well */
	    vflag++;
	    break;
	}
    }

    if (rflag + fflag == 1 && inst != -999) {
	pmprintf("%s: -i and (-f or -p) are mutually exclusive\n", pmGetProgname());
	opts.errors++;
    }

    if (rflag == 0 && fflag == 0 && inst == -999) {
	/* default */
	inst = 0;
    }

    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	exitsts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(exitsts);
    }

    if (opts.context == PM_CONTEXT_ARCHIVE)
	source = opts.archives[0];
    else if (opts.context == PM_CONTEXT_HOST)
	source = opts.hosts[0];
    else if (opts.context == PM_CONTEXT_LOCAL)
	source = NULL;
    else {
	opts.context = PM_CONTEXT_HOST;
	source = "local:";
    }
    if ((sts = pmNewContext(opts.context, source)) < 0) {
	if (opts.context == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmGetProgname(), source, pmErrStr(sts));
	else if (opts.context == PM_CONTEXT_LOCAL)
	    fprintf(stderr, "%s: Cannot make local standalone connection: %s\n",
		pmGetProgname(), pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmGetProgname(), source, pmErrStr(sts));
	exit(1);
    }
    contextid = sts;

    if (opts.context == PM_CONTEXT_ARCHIVE) {
	if (pmGetContextOptions(contextid, &opts)) {
	    pmflush();
	    exit(1);
	}
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    while (opts.optind < argc) {
	printf("%s:\n", argv[opts.optind]);
	sts = dometric(argv[opts.optind]);
	if (sts < 0)
	    printf("Error: %s\n", pmErrStr(sts));
	opts.optind++;
    }

    exit(0);
}
