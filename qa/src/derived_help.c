/*
 * Copyright (c) 2016 Red Hat.
 * Copyright (c) 2022-23 Ken McDonell.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * Derived metric help text exerciser.
 *
 * Assumes metrics of interest match names qa.*
 */
#include <pcp/pmapi.h>
#include "libpcp.h"

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("options"),
    { "", 1, 'c', "FILE", "load derived metrics from config file" },
    PMAPI_OPTIONS_END
};
static pmOptions opts = {
    .short_options = "c:D:?",
    .long_options = longopts,
    .short_usage = "[options]",
};

static char	**namelist;
static int	numnames;

static void
dometric(const char *name)
{
    char	**tmp_namelist;
    tmp_namelist = (char **)realloc(namelist, (++numnames)*sizeof(namelist[0]));
    if (tmp_namelist == NULL) {
	fprintf(stderr, "dometric: realloc() for %d metrics failed\n", numnames);
	exit(1);
    }
    namelist = tmp_namelist;
    if ((namelist[numnames-1] = strdup(name)) == NULL) {
	fprintf(stderr, "dometric: namelist[%d]: strdup(%s) failed\n", numnames-1, name);
	exit(1);
    }
}

static int
compar(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    char	*p;
    char	*err;
    int		sts;
    pmID	pmid;
    const char	*name;
    char	*text;
    char	buf[128];

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'c':	/* global derived metrics config file */
	    sts = pmLoadDerivedConfig(opts.optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: -c %s: %s\n", pmGetProgname(), opts.optarg, pmErrStr(sts));
		exit(1);
	    }
	    break;
	}
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	return 1;
    }

    name = "qa.rd.none";
    if ((p = pmRegisterDerived(name, "sample.bin")) != NULL) {
	printf("Error: pmRegisterDerived(%s): %s\n", name, p);
	exit(1);
    }
    name = "qa.rd.oneline";
    if ((p = pmRegisterDerived(name, "sample.bin")) != NULL) {
	printf("Error: pmRegisterDerived(%s): %s\n", name, p);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - foo bar", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_ONELINE, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    name = "qa.rd.helptext";
    if ((p = pmRegisterDerived(name, "sample.bin")) != NULL) {
	printf("Error: pmRegisterDerived(%s): %s\n", name, p);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - mumble\nstumble\nfumble", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_HELP, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    name = "qa.rd.both";
    if ((p = pmRegisterDerived(name, "sample.bin")) != NULL) {
	printf("Error: pmRegisterDerived(%s): %s\n", name, p);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - foo bar", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_ONELINE, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - mumble\nstumble\nfumble", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_HELP, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }

    name = "qa.rdm.none";
    if ((sts = pmRegisterDerivedMetric(name, "sample.bin", &err)) < 0) { 
	printf("Error: pmRegisterDerivedMetric(%s): (%d) %s\n", name, sts, err);
	exit(1);
    }
    name = "qa.rdm.oneline";
    if ((sts = pmRegisterDerivedMetric(name, "sample.bin", &err)) < 0) {
	printf("Error: pmRegisterDerivedMetric(%s): (%d) %s\n", name, sts, err);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - foo bar", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_ONELINE, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    name = "qa.rdm.helptext";
    if ((sts = pmRegisterDerivedMetric(name, "sample.bin", &err)) < 0) {
	printf("Error: pmRegisterDerivedMetric(%s): (%d) %s\n", name, sts, err);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - mumble\nstumble\nfumble", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_HELP, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    name = "qa.rdm.both";
    if ((sts = pmRegisterDerivedMetric(name, "sample.bin", &err)) < 0) {
	printf("Error: pmRegisterDerivedMetric(%s): (%d) %s\n", name, sts, err);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - foo bar", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_ONELINE, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - mumble\nstumble\nfumble", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_HELP, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	printf("Error: pmNewContext: %s\n", pmErrStr(sts));
	exit(1);
    }

    name = "qa.ad.none";
    if ((p = pmAddDerived(name, "sample.bin")) != NULL) {
	printf("Error: pmAddDerived(%s): %s\n", name, p);
	exit(1);
    }
    name = "qa.ad.oneline";
    if ((p = pmAddDerived(name, "sample.bin")) != NULL) {
	printf("Error: pmAddDerived(%s): %s\n", name, p);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - foo bar", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_ONELINE, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    name = "qa.ad.helptext";
    if ((p = pmAddDerived(name, "sample.bin")) != NULL) {
	printf("Error: pmAddDerived(%s): %s\n", name, p);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - mumble\nstumble\nfumble", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_HELP, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    name = "qa.ad.both";
    if ((p = pmAddDerived(name, "sample.bin")) != NULL) {
	printf("Error: pmAddDerived(%s): %s\n", name, p);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - foo bar", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_ONELINE, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - mumble\nstumble\nfumble", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_HELP, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }

    name = "qa.ram.none";
    if ((sts = pmAddDerivedMetric(name, "sample.bin", &err)) < 0) { 
	printf("Error: pmRegisterDerivedMetric(%s): (%d) %s\n", name, sts, err);
	exit(1);
    }
    name = "qa.ram.oneline";
    if ((sts = pmAddDerivedMetric(name, "sample.bin", &err)) < 0) {
	printf("Error: pmRegisterDerivedMetric(%s): (%d) %s\n", name, sts, err);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - foo bar", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_ONELINE, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    name = "qa.ram.helptext";
    if ((sts = pmAddDerivedMetric(name, "sample.bin", &err)) < 0) {
	printf("Error: pmRegisterDerivedMetric(%s): (%d) %s\n", name, sts, err);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - mumble\nstumble\nfumble", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_HELP, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    name = "qa.ram.both";
    if ((sts = pmAddDerivedMetric(name, "sample.bin", &err)) < 0) {
	printf("Error: pmRegisterDerivedMetric(%s): (%d) %s\n", name, sts, err);
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - foo bar", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_ONELINE, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }
    snprintf(buf, sizeof(buf), "i am %s - mumble\nstumble\nfumble", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_HELP, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s): %s\n", name, pmErrStr(sts));
	exit(1);
    }

    printf("\nSome error cases ...\n");

    name = "qa.ram.both";
    snprintf(buf, sizeof(buf), "i am %s - duplicate", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_ONELINE, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s, ONELINE): %s\n", name, pmErrStr(sts));
    }
    if ((sts = pmAddDerivedText(name, PM_TEXT_HELP, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s, HELP): %s\n", name, pmErrStr(sts));
    }
    name = "qa.no.such.metric";
    snprintf(buf, sizeof(buf), "i am %s - duplicate", name);
    if ((sts = pmAddDerivedText(name, PM_TEXT_ONELINE, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s, ONELINE): %s\n", name, pmErrStr(sts));
    }
    if ((sts = pmAddDerivedText(name, PM_TEXT_HELP, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s, HELP): %s\n", name, pmErrStr(sts));
    }
    name = "qa.ram.both";
    if ((sts = pmAddDerivedText(name, 12345, buf)) < 0) {
	printf("Error: pmAddDerivedText(%s, 12345): %s\n", name, pmErrStr(sts));
    }
    pmid = pmID_build(29,0,6);	/* sample.bin */
    if ((sts = pmLookupText(pmid, 12345, &p)) < 0) {
	printf("Error: LookupText(..., 12345): %s\n", pmErrStr(sts));
    }

    /* enumerate all the qa.* metrics and sort 'em */
    if ((sts = pmTraversePMNS("qa", dometric)) < 0) {
	printf("Error: pmTraversePMNS: %s\n", pmErrStr(sts));
	exit(1);
    }
    qsort(namelist, numnames, sizeof(namelist[0]), compar);

    printf("\n%d QA names to work with ,,,\n", numnames);

    for (i = 0; i < numnames; i++) {
	putchar('\n');
	name = namelist[i];
	if ((sts = pmLookupName(1, &name, &pmid)) < 0) {
	    printf("Error: pmLookupName(%s): %s\n", name, pmErrStr(sts));
	    exit(1);
	}
	if ((sts = pmLookupText(pmid, PM_TEXT_ONELINE, &text)) < 0)
	    printf("%s[oneline]: %s\n", name, pmErrStr(sts));
	else {
	    printf("%s[oneline] %s\n", name, text);
	    free(text);
	}
	if ((sts = pmLookupText(pmid, PM_TEXT_HELP, &text)) < 0)
	    printf("%s[helptext]: %s\n", name, pmErrStr(sts));
	else {
	    printf("%s[helptext]\n%s\n", name, text);
	    free(text);
	}
    }

    return 0;
}
