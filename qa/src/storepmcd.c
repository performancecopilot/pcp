/*
 * Copyright (c) 2015 Red Hat.
 * 
 * Test helper program for exercising pmcd.client metrics
 * (pmcd.client.whoami and pmcd.client.container).
 */

#include <ctype.h>
#include <assert.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

enum {
    pmcd_hostname,
    pmcd_client_whoami,
    pmcd_client_container,

    num_metrics
};

char *namelist[] = {
    "pmcd.hostname",
    "pmcd.client.whoami",
    "pmcd.client.container"
};

pmResult *
fetch_values(pmID *pmids)
{
    pmResult	*result;
    int		sts;

    if ((sts = pmFetch(num_metrics, pmids, &result)) < 0) {
	fprintf(stderr, "pmFetch: %s\n", pmErrStr(sts));
	exit(1);
    }
    return result;
}

void
exclude_values(pmID pmid, int nexcludes, int *excludes)
{
    pmDesc	desc;
    int		sts;

    if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
	fprintf(stderr, "pmLookupDesc: %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmAddProfile(desc.indom, 0, NULL)) < 0) { /* all on */
	fprintf(stderr, "pmAddProfile: %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmDelProfile(desc.indom, nexcludes, excludes)) < 0) {
	fprintf(stderr, "pmDelProfile: %s\n", pmErrStr(sts));
	exit(1);
    }
}

void
report_values(pmResult *result)
{
    int		i, j;

    assert(result->numpmid == num_metrics);
    for (i = 0; i < num_metrics; i++) {
	pmValueSet	*vsp;

	vsp = result->vset[i];
	for (j = 0; j < vsp->numval; j++) {
	    char	*value;
	    int		inst;

	    value = vsp->vlist[j].value.pval->vbuf;
	    inst  = vsp->vlist[j].inst;
	    if (i == 0)
		printf("%s: \"%s\"\n", namelist[i], value);
	    else
		printf("%s[%d]: \"%s\"\n", namelist[i], inst, value);
	}
    }
    printf("\n");
}

void
store_container(pmID pmid, char *name)
{
    pmResult            store;
    pmValueSet          pmvs;
    pmValueBlock        *pmvb;
    int			vlen;
    int			sts;

    vlen = PM_VAL_HDR_SIZE + strlen(name) + 1;
    pmvb = (pmValueBlock *)calloc(1, vlen);
    if (pmvb == NULL)
	pmNoMem("store_container", vlen, PM_FATAL_ERR);
    pmvb->vtype = PM_TYPE_STRING;
    pmvb->vlen = vlen;
    strcpy(pmvb->vbuf, name);

    pmvs.pmid = pmid;
    pmvs.numval = 1;
    pmvs.valfmt = PM_VAL_SPTR;
    pmvs.vlist[0].value.pval = pmvb;
    pmvs.vlist[0].inst = PM_IN_NULL;

    memset(&store, 0, sizeof(store));
    store.numpmid = 1;
    store.vset[0] = &pmvs;
    if ((sts = pmStore(&store)) < 0) {
	fprintf(stderr, "pmStore: %s\n", pmErrStr(sts));
	exit(1);
    }
    printf("pmStore: container \"%s\" ok\n", name);
    free(pmvb);
}

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		sts;
    int		errflag = 0;
    char	*host = "local:";
    static char	*usage = "[-w] [-x N] [-D debugspec] [-h hostname] container...";
    pmID	pmids[num_metrics];
    pmResult	*result;
    int		*excludes = NULL;
    int		excludesize = 0;
    int		nexcludes = 0;
    int		whoflag = 0;
    int		delay = 0;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "d:D:h:wx:?")) != EOF) {
	switch (c) {
	case 'D':	/* debug options */
	    if ((sts = pmSetDebug(optarg)) < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'd':	/* sleep interval after changing container */
	    delay = atoi(optarg);
	    break;

	case 'h':	/* hostname for PMCD to contact */
	    host = optarg;
	    break;

	case 'w':	/* set pmcd.client.whoami metric */
	    whoflag = 1;
	    break;

	case 'x':	/* exclude instance from returned values */
	    excludesize = (nexcludes + 1) * sizeof(int);
	    if ((excludes = realloc(excludes, excludesize)) == NULL)
		pmNoMem("excludes", excludesize, PM_FATAL_ERR);
	    excludes[nexcludes++] = atoi(optarg);
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	printf("Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
	printf("%s: Cannot connect to PMCD on host \"%s\": %s\n", pmGetProgname(), host, pmErrStr(sts));
	exit(1);
    }

    if (whoflag)
	__pmSetClientIdArgv(argc, argv);

    if ((sts = pmLookupName(num_metrics, namelist, pmids)) < 0) {
	fprintf(stderr, "pmLookupName: %s\n", pmErrStr(sts));
	for (i = 0; i < num_metrics; i++) {
	    if (pmids[i] == PM_ID_NULL)
		fprintf(stderr, "   %s is bad\n", namelist[i]);
	}
	exit(1);
    }

    if (nexcludes)
	exclude_values(pmids[pmcd_client_whoami], nexcludes, excludes);

    result = fetch_values(&pmids[0]);
    report_values(result);
    pmFreeResult(result);

    /* iterate over remaining arguments (containers) and switch */
    for (i = optind; i < argc; i++) {
	if (i != 0 && delay) {
	    printf("[delay %d sec]\n", delay);
	    sleep(delay);
	}
	store_container(pmids[pmcd_client_container], argv[i]);
	result = fetch_values(pmids);
	report_values(result);
	pmFreeResult(result);
    }
    exit(0);
}
