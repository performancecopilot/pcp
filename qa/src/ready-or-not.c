/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2023 Ken McDonell.  All Rights Reserved.
 *
 * pmcd makes these calls to communicate with a PMDA ... all of
 * them are exposed to ready-not-ready protocol issues:
 *   [ ] __pmSendAttr
 *   [y] __pmSendChildReq - pmGetChildren() or pmGetChildrenStatus()
 *   [ ] __pmSendCreds
 *   [y] __pmSendDescReq - pmLookupDesc()
 *   [ ] __pmSendError
 *   [y] __pmSendFetch - pmFetch()
 *   [y] __pmSendHighResResult - pmFetchHighRes()
 *   [y] __pmSendIDList - pmNameID() or pmNameAll()
 *   [y]                - pmLookupDescs()
 *   [ ] __pmSendInstanceReq
 *   [ ] __pmSendLabelReq
 *   [y] __pmSendNameList - pmLookupName
 *   [ ] __pmSendProfile
 *   [ ] __pmSendResult - pmStore
 *   [ ] __pmSendTextReq
 *   [y] __pmSendTraversePMNSReq - pmTraversePMNS, pmTraversePMNS_r
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
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "[options]",
};

struct {
    const char	*name;
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
    { "sample.secret.family",		/* indom, dynamic, leaf */
      PM_ID_NULL, PM_INDOM_NULL, PM_IN_NULL, NULL },
    { "pmcd.seqnum",			/* leaf, always present and available */
      PM_ID_NULL, PM_INDOM_NULL, PM_IN_NULL, NULL },
};

pmID	pmidlist[10];

int	magic;

struct {
    pmID	pmid;
    const char	*name;
} ctl_pmns[] = {
    { PM_ID_NULL, "sample.secret.foo.bar.three" },	/* lots of aliases */
    { PM_ID_NULL, "pmcd.seqnum" },			/* no aliases, always preset */
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
	smack_rp->vset[0]->vlist[0].value.lval = 20;
    }

    if ((sts = pmStore(smack_rp)) < 0 && sts != PM_ERR_AGAIN) {
	fprintf(stderr, "pmStore(%s): %s\n", pmIDStr(smack_rp->vset[0]->pmid), pmErrStr(sts));
	exit(1);
    }
}

static
void dometric(const char *name)
{
    printf("dometric(%s) called\n", name);
}

static
void dometric_r(const char *name, void *closure)
{
    int		*ip;
    printf("dometric_r(%s) called\n", name);
    ip = (int *)closure;
    if (*ip != magic)
	printf("Botch: closure %d, not %d as expected\n", *ip, magic);
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
    struct timeval	delay = { 0, 20000 };	/* 20msec pause */

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
     * initialization ... lookup all the ctl[] names we can
     */
    for (i = 0; i < sizeof(ctl) / sizeof(ctl[0]); i++) {
	sts = pmLookupName(1, (const char **)&ctl[i].name, &pmid);
	if (sts < 0) {
	    printf("ctl[%d] name  %s LookupName Error: %s\n", i, ctl[i].name, pmErrStr(sts));
	    pmidlist[i] = ctl[i].pmid = PM_ID_NULL;
	    continue;
	}
	pmidlist[i] = ctl[i].pmid = pmid;
	/* get the pmDesc's now */
	sts = pmLookupDesc(pmid, &desc);
	if (sts < 0) {
	    printf("ctl[%d] name  %s LookupDesc Error: %s\n", i, ctl[i].name, pmErrStr(sts));
	    continue;
	}
	ctl[i].indom = desc.indom;
	if (desc.indom != PM_INDOM_NULL) {
	    int		*instlist;
	    char	**namelist;
	    sts = pmGetInDom(desc.indom, &instlist, &namelist);
	    if (sts < 0)
		printf("ctl[%d] name  %s GetIndom(%s) Error: %s\n", i, ctl[i].name, pmInDomStr(desc.indom), pmErrStr(sts));
	    if (sts >= 1) {
		ctl[i].inst = instlist[sts/2];
		ctl[i].iname = strdup(namelist[sts/2]);
		// be brave, assume strdup() works
		free(instlist);
		free(namelist);
	    }
	}
	printf("ctl[%d] name  %s pmid %s", i, ctl[i].name, pmIDStr(pmid));
	if (desc.indom != PM_INDOM_NULL) {
	    printf(" indom %s", pmInDomStr(ctl[i].indom));
	    if (ctl[i].iname != NULL)
		printf(" {%d, \"%s\"}", ctl[i].inst, ctl[i].iname);
	}
	putchar('\n');
    }

    /*
     * initialization ... lookup all the ctl_pmns[] names we can
     */
    for (i = 0; i < sizeof(ctl_pmns) / sizeof(ctl_pmns[0]); i++) {
	sts = pmLookupName(1, (const char **)&ctl_pmns[i].name, &pmid);
	if (sts < 0) {
	    printf("ctl_pmns[%d] name  %s LookupName Error: %s\n", i, ctl_pmns[i].name, pmErrStr(sts));
	    continue;
	}
	ctl_pmns[i].pmid = pmid;
	printf("ctl_pmns[%d] name  %s pmid %s\n", i, ctl_pmns[i].name, pmIDStr(pmid));
    }

    /*
     * Outer loop is: default (ready), not ready
     */
    for (limbo = 0; limbo < 2; limbo++) {
	if (limbo)
	    smack();
	/*
	 * Inner loop is: 1 try when ready, 2 tries when not ready (with
	 * short sleep between tries, so expect 2nd try to succeed)
	 */
	for (j = 0; j <= limbo; j++) {
	    char	**offspring;
	    int		*status;
	    char	*name;
	    char	**nameset;
	    pmDesc	desclist[10];

	    /*
	     * fetch ops:
	     *   pmFetch, pmFetchHighRes
	     */
	    for (i = 0; i < sizeof(ctl) / sizeof(ctl[0]); i++) {
		if (ctl[i].pmid != PM_ID_NULL) {
		    /* pmFetch */
			printf("ctl[%d][%s] name %s pmFetch ...\n", i, (limbo && j == 0) ? "notready" : "ok", ctl[i].name);
			sts = pmFetch(1, &ctl[i].pmid, &rp);
			if (sts < 0)
			    printf("Error: %s\n", pmErrStr(sts));
			else {
			    __pmDumpResult(stdout, rp);
			    pmFreeResult(rp);
			}
		    /* pmFetchHighRes */
		    printf("ctl[%d][%s] name %s pmFetchHighRes ...\n", i, (limbo && j == 0) ? "notready" : "ok", ctl[i].name);
		    sts = pmFetchHighRes(1, &ctl[i].pmid, &hrp);
		    if (sts < 0)
			printf("Error: %s\n", pmErrStr(sts));
		    else {
			__pmDumpHighResResult(stdout, hrp);
			pmFreeHighResResult(hrp);
		    }
		}
	    }

	    /*
	     * pmDesc ops:
	     *    pmLookupDesc, pmLookupDescs
	     */
	    for (i = 0; i < sizeof(ctl) / sizeof(ctl[0]); i++) {
		printf("ctl[%d][%s] pmid %s pmLookupDesc ...\n", i, (limbo && j == 0) ? "notready" : "ok", pmIDStr(ctl[i].pmid));
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
		if (i > 0)
		    continue;
		
		/*
		 * this one will not return PM_ERR_AGAIN, just PM_ID_NULL
		 * in the pmDesc for the ones that could not be looked up
		 */
		printf("[%s] pmLookupDescs ... ", (limbo && j == 0) ? "notready" : "ok");
		sts = pmLookupDescs(sizeof(ctl) / sizeof(ctl[0]), pmidlist, desclist);
		if (sts < 0)
		    printf("Error: %s\n", pmErrStr(sts));
		else {
		    int		k, ok = 1;
		    printf("%d descs\n", sts);
		    for (k = 0; k < sizeof(ctl) / sizeof(ctl[0]); k++) {
			if (desclist[k].pmid != PM_ID_NULL && desclist[k].pmid != pmidlist[k]) {
			    printf("Botch: returned [%d] pmid (%s)", k, pmIDStr(desclist[k].pmid));
			    printf(" != expected pmid (%s)\n", pmIDStr(pmidlist[k]));
			    ok = 0;
			}
		    }
		    if (ok)
			printf("OK\n");
		}
	    }

	    /*
	     * PMNS ops:
	     *	  pmGetChildren, pmGetChildrenStatus, pmNameID, pmNameAll,
	     *	  pmLookupName, pmTraversePMNS, pmTraversePMNS_r
	     *
	     * Need mostly dynamic metrics so pmcd is forced to ship the
	     * request to the PMDA instead of answering from the loaded
	     * PMNS.
	     */

	    printf("pmGetChildren(sample.secret) ...\n");
	    sts = pmGetChildren("sample.secret", &offspring);
	    if (sts < 0)
		printf("Error: %s\n", pmErrStr(sts));
	    else {
		int	k;
		for (k = 0; k < sts; k++)
		    printf("pmns child[%d] %s\n", k, offspring[k]);
		free(offspring);
	    }

	    printf("pmGetChildrenStatus(sample.secret) ...\n");
	    sts = pmGetChildrenStatus("sample.secret", &offspring, &status);
	    if (sts < 0)
		printf("Error: %s\n", pmErrStr(sts));
	    else {
		int	k;
		for (k = 0; k < sts; k++)
		    printf("pmns child[%d] %d %s\n", k, status[k], offspring[k]);
		free(offspring);
		free(status);
	    }

	    for (i = 0; i < sizeof(ctl_pmns) / sizeof(ctl_pmns[0]); i++) {
		printf("pmNameID(%s) ...\n", pmIDStr(ctl_pmns[i].pmid));
		sts = pmNameID(ctl_pmns[i].pmid, &name);
		if (sts < 0)
		    printf("Error: PMID: %s: %s\n", pmIDStr(ctl_pmns[i].pmid), pmErrStr(sts));
		else {
		    if (strcmp(name, ctl_pmns[i].name) == 0)
			printf("name %s\n", name);
		    else {
			printf("Botch: returned name (%s)", name);
			printf(" != expected name (%s)\n", ctl_pmns[i].name);
		    }
		    free(name);
		}
		printf("pmNameAll(%s) ...\n", pmIDStr(ctl_pmns[i].pmid));
		sts = pmNameAll(ctl_pmns[i].pmid, &nameset);
		if (sts < 0)
		    printf("Error: PMID: %s: %s\n", pmIDStr(ctl_pmns[i].pmid), pmErrStr(sts));
		else {
		    int	k;
		    for (k = 0; k < sts; k++) {
			printf("name[%d] %s\n", k, nameset[k]);
		    }
		    free(nameset);
		}
		printf("pmLookupName(%s) ...\n", ctl_pmns[i].name);
		sts = pmLookupName(1, (const char **)&ctl_pmns[i].name, &pmid);
		if (sts < 0)
		    printf("Error: name: %s: %s\n", ctl_pmns[i].name, pmErrStr(sts));
		else {
		    if (pmid == ctl_pmns[i].pmid)
			printf("pmid %s\n", pmIDStr(pmid));
		    else {
			printf("Botch: returned pmid (%s)", pmIDStr(pmid));
			printf(" != expected pmid (%s)\n", pmIDStr(ctl_pmns[i].pmid));
		    }
		}
	    }
	    for (i = 0; i < sizeof(ctl) / sizeof(ctl[0]); i++) {
		if (ctl[i].pmid != PM_ID_NULL) {
		    /* leaf in PMNS, skip this one ... */
		    continue;
		}
		printf("pmTraversePMNS(%s) ...\n", ctl[i].name);
		sts = pmTraversePMNS(ctl[i].name, dometric);
		if (sts < 0)
		    printf("Error: %s\n", pmErrStr(sts));
		else
		    printf("=> %d\n", sts);
		magic = 1230 + i;
		printf("pmTraversePMNS_r(%s, %d) ...\n", ctl[i].name, magic);
		sts = pmTraversePMNS_r(ctl[i].name, dometric_r, &magic);
		if (sts < 0)
		    printf("Error: %s\n", pmErrStr(sts));
		else
		    printf("=> %d\n", sts);
	    }

	    if (limbo && j == 0)
		__pmtimevalSleep(delay);
	}
    }

    return 0;
}
