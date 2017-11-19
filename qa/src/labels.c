/*
 * Copyright (c) 2017 Red Hat.
 *
 * Test helper program for exercising pmLookupLabels(3).
 */

#include <ctype.h>
#include <assert.h>
#include <pcp/pmapi.h>

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_ARCHIVE,
    PMOPT_HOST,
    PMOPT_CONTAINER,
    PMOPT_LOCALPMDA,
    PMOPT_SPECLOCAL,
    PMOPT_ORIGIN,
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "a:D:h:K:LO:VxzZ:?",
    .long_options = longopts,
    .short_usage = "[options] metric [...]",
};

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		sts;
    int		flags;
    int		nsets;
    int		exitsts;
    int		nmetrics;
    char	*metric;
    char	*source;
    pmID	pmid;
    pmLabelSet	*lsp;
    pmLabelSet	*sets;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	    default:
		opts.errors++;
		break;
	}
    }

    nmetrics = argc - opts.optind;
    if (opts.errors || nmetrics < 1 || (opts.flags & PM_OPTFLAG_EXIT)) {
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
	    fprintf(stderr, "%s: Cannot make standalone connection on localhost: %s\n",
			    pmGetProgname(), pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
			    pmGetProgname(), source, pmErrStr(sts));
	exit(1);
    }
    /* complete TZ and time window option (origin) setup */
    if (opts.context == PM_CONTEXT_ARCHIVE && pmGetContextOptions(sts, &opts)) {
	pmflush();
	exit(1);
    }

    for (c = 0; c < nmetrics; c++) {
	metric = argv[opts.optind++];
	if ((sts = pmLookupName(1, &metric, &pmid)) < 0) {
	    fprintf(stderr, "%s: cannot lookup name %s: %s\n",
			pmGetProgname(), metric, pmErrStr(sts));
	    exit(1);
	}
	if (pmid == PM_ID_NULL) {
	    fprintf(stderr, "%s: no such metric: %s\n", pmGetProgname(), metric);
	    continue;
	}

	if ((sts = nsets = pmLookupLabels(pmid, &sets)) < 0) {
	    fprintf(stderr, "%s: cannot get labels for %s: %s\n",
			pmGetProgname(), metric, pmErrStr(sts));
	    exit(1);
	}

	printf("%s labels from %d sets:\n", metric, nsets);
	for (i = 0; i < nsets; i++) {
	    lsp = &sets[i];
	    if (lsp->nlabels <= 0 || lsp->jsonlen == 0)
		continue;
	    flags = lsp->labels[0].flags;
	    if (flags & PM_LABEL_CONTEXT)
		printf("  Context:\t%.*s\n", lsp->jsonlen, lsp->json);
	    else if (flags & PM_LABEL_DOMAIN)
		printf("  Domain:\t%.*s\n", lsp->jsonlen, lsp->json);
	    else if (flags & PM_LABEL_INDOM)
		printf("  InDom:\t%.*s\n", lsp->jsonlen, lsp->json);
	    else if (flags & PM_LABEL_CLUSTER)
		printf("  Cluster:\t%.*s\n", lsp->jsonlen, lsp->json);
	    else if (flags & PM_LABEL_ITEM)
		printf("  Item:\t%.*s\n", lsp->jsonlen, lsp->json);
	    else if (flags & PM_LABEL_INSTANCES)
		printf("  Inst[%u]:\t%.*s\n", lsp->inst, lsp->jsonlen, lsp->json);
	    else
		printf("  ???[0x%x]:\t%.*s\n", flags, lsp->jsonlen, lsp->json);
	}
	pmFreeLabelSets(sets, nsets);
    }

    exit(0);
}
