/*
 * Copyright (c) 2023 Red Hat.
 *
 * Test helper program for exercising libpcp label parsing.
 */

#include <ctype.h>
#include <pcp/pmapi.h>

/* libpcp internal routine used by this test code */
PCP_CALL extern int __pmAddLabels(pmLabelSet **, const char *, int);

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "[options] json",
};

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		sts;
    int		nsets;
    char	json[PM_MAXLABELJSONLEN] = {0};
    size_t	jsonlen = 0;
    pmLabelSet	*lsp;
    pmLabelSet	*sets = NULL;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	    default:
		opts.errors++;
		break;
	}
    }

    for (c = opts.optind; c <= argc - opts.optind; c++) {
	if (jsonlen + strlen(argv[c]) + 2 >= sizeof(json)) {
	    fprintf(stderr, "Command line exceeds labelset maximum\n");
	    exit(1);
	}
	if (c == opts.optind) {
	    strcpy(json, argv[c]);
	} else {
	    strcat(json, " ");
	    strcat(json, argv[c]);
	}
    }

    printf("Parser input: %s\n", json);
    if ((sts = nsets = __pmAddLabels(&sets, json, 0)) < 0) {
        fprintf(stderr, "__pmAddLabels failed: %s\n", pmErrStr(sts));
	sts = 1;
    } else {
	lsp = &sets[0];
	printf("Parsed %s labels (len=%d) with %d labels:\n", lsp->json, lsp->jsonlen, lsp->nlabels);
	for (i = 0; i < lsp->nlabels; i++) {
	    pmLabel *lp = &lsp->labels[i];
	    printf("[%d] %.*s=%.*s\n", i, lp->namelen, lsp->json+lp->name, lp->valuelen, lsp->json+lp->value);
	}
	pmFreeLabelSets(sets, 1);
	sts = 0;
    }

    return sts;
}
