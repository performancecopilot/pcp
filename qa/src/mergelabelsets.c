/*
 * Copyright (c) 2017 Red Hat.
 *
 * Test program for exercising pmMergeLabelSets(3).
 */

#include <ctype.h>
#include <assert.h>
#include <pcp/pmapi.h>

#define LOCATION \
    "{\"datacenter\":\"torquay\",\"environment\":\"production\"}"

static pmLabel location_labels[] = {
    { .name = 2,   .namelen = 10,
      .value = 14, .valuelen = 9,
      .flags = PM_LABEL_CONTEXT|PM_LABEL_OPTIONAL },
    { .name = 25,  .namelen = 11,
      .value = 38, .valuelen = 12,
      .flags = PM_LABEL_CONTEXT },
};
static pmLabelSet location = {
    .nlabels = 2,
    .json = LOCATION,
    .jsonlen = sizeof(LOCATION),
    .labels = location_labels,
};

#define SERVICES \
    "{\"services\":[\"indexer\",\"database\"]}"

static pmLabel services_labels[] = {
    { .name = 2,   .namelen = 8,
      .value = 12, .valuelen = 22,
      .flags = PM_LABEL_DOMAIN },
};
static pmLabelSet services = {
    .nlabels = 1,
    .json = SERVICES,
    .jsonlen = sizeof(SERVICES),
    .labels = services_labels,
};

#define TESTING \
    "{\"more\":{\"all\":false,\"none\":true},\"none\":none,\"some\":[1,2,3]}"

static pmLabel testing_labels[] = {
    { .name = 2,   .namelen = 4,
      .value = 8, .valuelen = 25,
      .flags = PM_LABEL_ITEM|PM_LABEL_OPTIONAL },
    { .name = 35,  .namelen = 4,
      .value = 41, .valuelen = 4,
      .flags = PM_LABEL_ITEM|PM_LABEL_OPTIONAL },
    { .name = 47,  .namelen = 4,
      .value = 53, .valuelen = 7,
      .flags = PM_LABEL_ITEM|PM_LABEL_OPTIONAL },
};

static pmLabelSet testing = {
    .nlabels = 3,
    .json = TESTING,
    .jsonlen = sizeof(TESTING),
    .labels = testing_labels,
};

#define EMPTY "{}"
static pmLabel empty_labels[1];
static pmLabelSet empty = {
    .nlabels = 0,
    .json = EMPTY,
    .jsonlen = sizeof(EMPTY),
    .labels = empty_labels,
};

static int
cull_optional(const pmLabel *lp, const char *json, void *arg)
{
    char	*testcase = (char *)arg;
    int		filter = !(lp->flags & PM_LABEL_OPTIONAL);

    if (pmDebugOptions.labels)
	fprintf(stderr, "%s cull_optional label %.*s(%.*s): %s\n",
		testcase,
		lp->namelen, &json[lp->name],
		lp->valuelen, &json[lp->value],
		filter ? "yes" : "no");

    return filter;
}

static int
cull_none(const pmLabel *lp, const char *json, void *arg)
{
    char	*testcase = (char *)arg;
    int		filter = 0;

    if (lp->valuelen == 4)
	filter = (strncmp(&json[lp->value], "none", 4) != 0);

    if (pmDebugOptions.labels)
	fprintf(stderr, "%s cull_none label %.*s(%.*s): %s\n",
		testcase,
		lp->namelen, &json[lp->name],
		lp->valuelen, &json[lp->value],
		filter ? "yes" : "no");

    return filter;
}


int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	json[PM_MAXLABELJSONLEN];
    void	*test;
    pmLabelSet	*sets[5];

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {
	case 'D':	/* debug flag */
	    if ((sts = pmSetDebug(optarg)) < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag)
	exit(1);

    sets[0] = &testing;
    sets[1] = &services;
    sets[2] = &location;
    test = (void *)"test-1";
    sts = pmMergeLabelSets(sets, 3, json, sizeof(json), NULL, test);
    if (sts < 0)
	fprintf(stderr, "pmMergeLabelSets: %s\n", pmErrStr(sts));
    else
	printf("Merged testing/services/location (no filter)\n%s\n\n", json);

    sets[0] = &services;
    sets[1] = &location;
    test = (void *)"test-2";
    sts = pmMergeLabelSets(sets, 2, json, sizeof(json), cull_optional, test);
    if (sts < 0)
	fprintf(stderr, "pmMergeLabelSets: %s\n", pmErrStr(sts));
    else
	printf("Merged services/location (cull optional)\n%s\n\n", json);

    sets[0] = &services;
    sets[1] = NULL;
    sets[2] = &location;
    sets[3] = &empty;
    test = (void *)"test-3";
    sts = pmMergeLabelSets(sets, 4, json, sizeof(json), cull_none, test);
    if (sts < 0)
	fprintf(stderr, "pmMergeLabelSets: %s\n", pmErrStr(sts));
    else
	printf("Merged services/NULL/location/empty (cull none)\n%s\n\n", json);

    exit(0);
}
