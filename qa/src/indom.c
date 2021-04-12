/*
 * indom - exercise pmGetInDom, pmNameInDom and pmLookupInDom
 */

#include <pcp/pmapi.h>

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_ARCHIVE,
    PMOPT_DEBUG,
    PMOPT_HOST,
    { "inst", 1, 'i', "INST", "report this internal instance identifier" },
    { "probe", 0, 'p', NULL, "report 1st, last and quartile internal instances" },
    PMOPT_LOCALPMDA,
    PMOPT_SPECLOCAL,
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ,
    .short_options = "a:D:h:i:K:LrzZ:?",
    .long_options = longopts,
    .short_usage = "[options] metricname ...",
};

static int	inst = -999;

static int	rflag = 0;

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
    int		probe[5];
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
    if (rflag) {
	if (numinst > 0) {
	    probe[0] = 0;
	    probe[1] = 0.25*(numinst-1);
	    probe[2] = (numinst-1)/2;
	    probe[3] = 0.75*(numinst-1);
	    probe[4] = numinst-1;
	    nprobe = 5;
	}
	else
	    nprobe = -1;
    }
    else {
	nprobe = 1;
    }


    for (p = 0; p < nprobe; p++) {
	int	try;
	iname = "no match";
	if (rflag)
	    try = ilist[probe[p]];
	else
	    try = inst;
	printf("pm*InDom: inst=[%d]", try);
	if ((sts = pmNameInDom(desc.indom, try, &iname)) < 0)
	    printf(" {%s}\n", pmErrStr(sts));
	else {
	    printf(" iname=<%s> reverse lookup:", iname);
	    if ((sts = pmLookupInDom(desc.indom, iname)) < 0)
		printf(" {%s}\n", pmErrStr(sts));
	    else
		printf(" inst=[%d]\n", sts);
	    free(iname);
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
		printf("   [%d] <%s> [ordinal %d]\n", ilist[probe[p]], nlist[probe[p]], probe[p]);
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
	case 'i':	/* instance */
	    inst = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || inst < 0) {
		pmprintf("%s: -i requires numeric argument\n", pmGetProgname());
		opts.errors++;
	    }
	    break;
	case 'r':	/* pick some representative instacnces */
	    rflag++;
	    break;
	}
    }

    if (rflag == 0 && inst == -999)
	inst = 0;

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
