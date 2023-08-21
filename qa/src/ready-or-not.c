/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2023 Ken McDonell.  All Rights Reserved.
 *
 * pmcd makes these calls to communicate with a PMDA ... all of
 * them are exposed to ready-not-ready protocol issues:
 *   [ ] __pmSendAttr
 *   [ ] __pmSendChildReq
 *   [ ] __pmSendCreds
 *   [y] __pmSendDescReq - pmLookupDesc()
 *   [ ] __pmSendError
 *   [y] __pmSendFetch - pmFetch()
 *   [y] __pmSendHighResResult - pmFetchHighRes()
 *   [ ] __pmSendIDList
 *   [ ] __pmSendInstanceReq
 *   [ ] __pmSendLabelReq
 *   [ ] __pmSendNameList
 *   [ ] __pmSendProfile
 *   [ ] __pmSendResult
 *   [ ] __pmSendTextReq
 *   [ ] __pmSendTraversePMNSReq
 *
 * ones marked [y] above are tested in the code below ...
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,	/* -D */
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_HEADER("template options"),
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "[options]",
};

struct {
    char	*name;
    pmID	pmid;
    pmInDom	indom;
    int		inst;
    char	*iname;
} ctl[] = {
    { "sample.long.hundred",		/* singular, static, leaf */
      PM_ID_NULL, PM_INDOM_NULL, PM_IN_NULL, NULL },
    { "sample.long",			/* singular, static, non-leaf */
      PM_ID_NULL, PM_INDOM_NULL, PM_IN_NULL, NULL },
    { "sample.secret.bar",		/* singular, dynamic, leaf */
      PM_ID_NULL, PM_INDOM_NULL, PM_IN_NULL, NULL },
    { "sample.secret.foo.bar.grunt",	/* singular, dynamic, non-leaf */
      PM_ID_NULL, PM_INDOM_NULL, PM_IN_NULL, NULL },
    { "sample.colour",			/* indom, static, leaf */
      PM_ID_NULL, PM_INDOM_NULL, PM_IN_NULL, NULL },
    { "sample.secret.family",		/* indom, dynamic, leaf */
      PM_ID_NULL, PM_INDOM_NULL, PM_IN_NULL, NULL },
};

static void
smack(void)
{
    static pmResult	*smack_rp = NULL;
    int			sts;

    if (smack_rp == NULL) {
	/* one trip initialization */
	pmID	pmid;
	char	*smack_name = "sample.not_ready_msec";

	if ((sts = pmLookupName(1, (const char **)&smack_name, &pmid)) < 0) {
	    fprintf(stderr, "pmLookupName(%s): %s\n", smack_name, pmErrStr(sts));
	    exit(1);
	}

	if ((sts = pmFetch(1, &pmid, &smack_rp)) < 0) {
	    fprintf(stderr, "pmFetch(%s): %s\n", pmIDStr(pmid), pmErrStr(sts));
	    exit(1);
	}
	/* store NOTREADY-to-READY delay in msec */
	smack_rp->vset[0]->vlist[0].value.lval = 5;
    }

    if ((sts = pmStore(smack_rp)) < 0 && sts != PM_ERR_AGAIN) {
	fprintf(stderr, "pmStore(%s): %s\n", pmIDStr(smack_rp->vset[0]->pmid), pmErrStr(sts));
	exit(1);
    }
}

int
main(int argc, char **argv)
{
    int		c;
    int		ctx;
    int		sts;
    int		i;
    int		j;
    int		limbo;		/* 0 => no games; 1 => notready-ready */
    pmID	pmid;
    pmDesc	desc;
    pmResult	*rp;
    pmHighResResult	*hrp;
    struct timeval	delay = { 0, 5000 };

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    /* non-flag args are argv[opts.optind] ... argv[argc-1] */
    while (opts.optind < argc) {
	printf("extra argument[%d]: %s\n", opts.optind, argv[opts.optind]);
	opts.optind++;
	opts.errors = 1;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if ((ctx = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "%s: Cannot connect to pmcd on localhost: %s\n",
		pmGetProgname(), pmErrStr(ctx));
	exit(EXIT_FAILURE);
    }

    /*
     * initialization ... lookup all the names we can */
    for (i = 0; i < sizeof(ctl) / sizeof(ctl[0]); i++) {
	sts = pmLookupName(1, (const char **)&ctl[i].name, &pmid);
	if (sts < 0) {
	    printf("[%d] name  %s LookupName Error: %s\n", i, ctl[i].name, pmErrStr(sts));
	    continue;
	}
	ctl[i].pmid = pmid;
	/* get the pmDesc's now */
	sts = pmLookupDesc(pmid, &desc);
	if (sts < 0) {
	    printf("[%d] name  %s LookupDesc Error: %s\n", i, ctl[i].name, pmErrStr(sts));
	    continue;
	}
	ctl[i].indom = desc.indom;
	if (desc.indom != PM_INDOM_NULL) {
	    int		*instlist;
	    char	**namelist;
	    sts = pmGetInDom(desc.indom, &instlist, &namelist);
	    if (sts < 0)
		printf("[%d] name  %s GetIndom(%s) Error: %s\n", i, ctl[i].name, pmInDomStr(desc.indom), pmErrStr(sts));
	    if (sts >= 1) {
		ctl[i].inst = instlist[sts/2];
		ctl[i].iname = strdup(namelist[sts/2]);
		// be brave, assume strdup() works
		free(instlist);
		free(namelist);
	    }
	}
	printf("[%d] name  %s pmid %s", i, ctl[i].name, pmIDStr(pmid));
	if (desc.indom != PM_INDOM_NULL) {
	    printf(" indom %s", pmInDomStr(ctl[i].indom));
	    if (ctl[i].iname != NULL)
		printf(" {%d, \"%s\"}", ctl[i].inst, ctl[i].iname);
	}
	putchar('\n');
    }

    for (limbo = 0; limbo < 2; limbo++) {
	for (i = 0; i < sizeof(ctl) / sizeof(ctl[0]); i++) {
	    if (ctl[i].pmid != PM_ID_NULL) {
		/* pmFetch */
		if (limbo)
		    smack();
		for (j = 0; j <= limbo; j++) {
		    printf("[%d][%s] name %s pmFetch ...\n", i, (limbo && j == 0) ? "notready" : "ok", ctl[i].name);
		    sts = pmFetch(1, &ctl[i].pmid, &rp);
		    if (sts < 0)
			printf("Error: %s\n", pmErrStr(sts));
		    else {
			__pmDumpResult(stdout, rp);
			pmFreeResult(rp);
		    }
		    if (limbo && j == 0)
			__pmtimevalSleep(delay);
		}
		/* pmFetchHighRes */
		if (limbo)
		    smack();
		for (j = 0; j <= limbo; j++) {
		    printf("[%d][%s] name %s pmFetchHighRes ...\n", i, (limbo && j == 0) ? "notready" : "ok", ctl[i].name);
		    sts = pmFetchHighRes(1, &ctl[i].pmid, &hrp);
		    if (sts < 0)
			printf("Error: %s\n", pmErrStr(sts));
		    else {
			__pmDumpHighResResult(stdout, hrp);
			pmFreeHighResResult(hrp);
		    }
		    if (limbo && j == 0)
			__pmtimevalSleep(delay);
		}
		/* pmLookupDesc */
		if (limbo)
		    smack();
		for (j = 0; j <= limbo; j++) {
		    printf("[%d][%s] pmid %s pmLookupDesc ...\n", i, (limbo && j == 0) ? "notready" : "ok", pmIDStr(ctl[i].pmid));
		    desc.pmid = PM_ID_NULL;
		    sts = pmLookupDesc(ctl[i].pmid, &desc);
		    if (sts < 0)
			printf("Error: %s\n", pmErrStr(sts));
		    else if (desc.pmid == ctl[i].pmid)
			printf("OK\n");
		    else {
			printf("Botch: returned pmid (%s)", pmIDStr(desc.pmid));
			printf(" != expected pmid (%s)\n", pmIDStr(ctl[i].pmid));
		    }
		    if (limbo && j == 0)
			__pmtimevalSleep(delay);
		}
	    }
	}
    }

    return 0;
}
