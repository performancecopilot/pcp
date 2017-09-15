/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

static fetchctl_t	*root;

static optreq_t		*req;
static pmDesc		*desc;
static int		*cost;
static int		*nfetch;
static pmUnits		nounits;

static void
setup(int i, int pmid_d, int pmid_i, int indom_d, int indom_s, int loinst, int hiinst)
{
    int			j;
    int			numinst;
    static int		pmid = 0;
    static int		indom = 0;
    static __pmID_int	*pmidp = (__pmID_int *)&pmid;
    static __pmInDom_int	*indomp = (__pmInDom_int *)&indom;

    pmidp->flag = 0;
    pmidp->domain = pmid_d;
    pmidp->cluster = 0;
    pmidp->item = pmid_i;
    indomp->flag = 0;
    indomp->domain = indom_d;
    indomp->serial = indom_s;

    desc = (pmDesc *)realloc(desc, (i+1) * sizeof(desc[0]));
    if (desc == (pmDesc *)0) {
	__pmNoMem("setup.desc", (i+1) * sizeof(desc[0]), PM_FATAL_ERR);
    }
    desc[i].pmid = pmid;
    desc[i].type = PM_TYPE_32;
    desc[i].indom = indom;
    desc[i].sem = PM_SEM_DISCRETE;
    desc[i].units = nounits;

    req = (optreq_t *)realloc(req, (i+1) * sizeof(req[0]));
    if (req == (optreq_t *)0) {
	__pmNoMem("setup.req", (i+1) * sizeof(req[0]), PM_FATAL_ERR);
    }
    if (loinst != -1) {
	req[i].r_numinst = numinst = (hiinst - loinst + 1);
	req[i].r_instlist = (int *)malloc(numinst * sizeof(req[i].r_instlist[0]));
	if (req[i].r_instlist == (int *)0) {
	    __pmNoMem("setup.instlist", numinst * sizeof(req[i].r_instlist[0]), PM_FATAL_ERR);
	}
	for (j = 0; j < numinst; j++)
	    req[i].r_instlist[j] = loinst + j;
    }
    else {
	/* use loinst == -1 to flag "all" instances here */
	req[i].r_numinst = 0;
	req[i].r_instlist = (int *)0;
    }

    cost = (int *)realloc(cost, (i+1) * sizeof(cost[0]));
    if (cost == (int *)0) {
	__pmNoMem("setup.cost", (i+1) * sizeof(cost[0]), PM_FATAL_ERR);
    }
    nfetch = (int *)realloc(nfetch, (i+1) * sizeof(nfetch[0]));
    if (nfetch == (int *)0) {
	__pmNoMem("setup.nfetch", (i+1) * sizeof(nfetch[0]), PM_FATAL_ERR);
    }
}

int
main(int argc, char **argv)
{
    int			c;
    int			errflag = 0;
    int			sts;
    static char		*usage = "[-D debugspec]";
    int			i;
    int			numreq;
    int			numfetch;
    int			totcost;
    int			dump;
    int			numfail;
    optcost_t		ocp = { 4, 1, 15, 10, 2, 0 };	/* my costs */
    fetchctl_t		*fp;

    __pmSetProgname(pmProgname);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {


	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    __pmOptFetchPutParams(&ocp);
    __pmOptFetchGetParams(&ocp);
    fprintf(stderr, "optFetch Cost Parameters:\n  pmid=%d indom=%d fetch=%d indomsize=%d, xtrainst=%d scope=%d\n",
		ocp.c_pmid, ocp.c_indom, ocp.c_fetch,
		ocp.c_indomsize, ocp.c_xtrainst, ocp.c_scope);

    i = 0;
    setup(i, 3, 4, 3, 7, 1, 3);
    nfetch[i] = 1;
    cost[i] = ocp.c_fetch + ocp.c_pmid + ocp.c_indom;
    setup(++i, 3, 4, 3, 7, -1, -1);
    nfetch[i] = 1;
    cost[i] = cost[i-1] - ocp.c_fetch;
    setup(++i, 3, 5, 3, 7, 1, 5);
    nfetch[i] = 1;
    cost[i] = ocp.c_pmid + ocp.c_indom + (ocp.c_indomsize - 5) * ocp.c_xtrainst;
    setup(++i, 3, 6, 3, 7, 1, 8);
    nfetch[i] = 1;
    cost[i] = cost[i-1] + (ocp.c_indomsize - 8) * ocp.c_xtrainst;
    setup(++i, 3, 7, 3, 7, 11, 11);
    nfetch[i] = 1;
    cost[i] = cost[i-1] + (ocp.c_indomsize - 1) * ocp.c_xtrainst;
    setup(++i, 4, 4, 3, 7, -1, -1);
    nfetch[i] = 1;
    cost[i] = cost[i-1] + ocp.c_pmid;
    setup(++i, 5, 4, 3, 7, 1, 5);
    nfetch[i] = 1;
    cost[i] = cost[i-1] + ocp.c_pmid + (ocp.c_indomsize - 5) * ocp.c_xtrainst;
    setup(++i, 6, 4, 3, 7, 1, 8);
    nfetch[i] = 1;
    cost[i] = cost[i-1] + ocp.c_pmid + (ocp.c_indomsize - 8) * ocp.c_xtrainst;
    setup(++i, 7, 4, 3, 7, 11, 11);
    nfetch[i] = 2;
    cost[i] = cost[i-1] + ocp.c_fetch + ocp.c_pmid + ocp.c_indom;
    setup(++i, 7, 4, 3, 7, 10, 12);
    nfetch[i] = 2;
    cost[i] = cost[i-1] - ocp.c_fetch;
    setup(++i, 7, 4, 3, 7, 9, 13);
    nfetch[i] = 2;
    cost[i] = cost[i-1];

    numreq = ++i;

    numfail = 0;
    for (i = 0; i < numreq; i++) {
	req[i].r_desc = &desc[i];
	__pmOptFetchAdd(&root, &req[i]);
	if (pmDebugOptions.optfetch) {
	    fprintf(stdout, "\nAdd request %d @ " PRINTF_P_PFX "%p\n", i, &req[i]);
	    __pmOptFetchDump(stdout, root);
	}
	numfetch = 0;
	totcost = 0;
	for (fp = root; fp != (fetchctl_t *)0; fp = fp->f_next) {
	    fp->f_state &= (~ OPT_STATE_UMASK);
	    totcost += fp->f_cost;
	    numfetch++;
	}
	dump = 0;
	if (numfetch != nfetch[i]) {
	    printf("After adding request %d, no. fetches %d, expected %d\n",
		i, numfetch, nfetch[i]);
	    dump = 1;
	}
	if (totcost != cost[i]) {
	    printf("After adding request %d, total cost %d, expected %d\n",
		i, totcost, cost[i]);
	    dump = 1;
	}
	if (dump == 1) {
	    numfail++;
	    __pmOptFetchDump(stdout, root);
	}
    }

    printf("Passed %d of %d addition tests\n", numreq - numfail, numreq);

    numfail = 0;
    for (i = numreq-1; i >= 0; i--) {
	__pmOptFetchDel(&root, &req[i]);
	if (pmDebugOptions.optfetch) {
	    fprintf(stdout, "\nDelete request %d @ " PRINTF_P_PFX "%p\n", i, &req[i]);
	    __pmOptFetchDump(stdout, root);
	}
	if (i == 0)
	    continue;
	numfetch = 0;
	totcost = 0;
	for (fp = root; fp != (fetchctl_t *)0; fp = fp->f_next) {
	    fp->f_state &= (~ OPT_STATE_UMASK);
	    totcost += fp->f_cost;
	    numfetch++;
	}
	/* handle special costs when we added another fetch to the group */
	if (i == 1)
	    totcost += ocp.c_fetch;
	else if (nfetch[i-1] != nfetch[i-2])
	    totcost += ocp.c_fetch * (nfetch[i-1] - nfetch[i-2]);

	dump = 0;
	if (numfetch != nfetch[i-1]) {
	    printf("After deleting data set %d, no. fetches %d, expected %d\n",
		i, numfetch, nfetch[i-1]);
	    dump = 1;
	}
	if (totcost != cost[i-1]) {
	    printf("After deleting request %d, total cost %d, expected %d\n",
		i, totcost, cost[i-1]);
	    dump = 1;
	}
	if (dump == 1) {
	    numfail++;
	    __pmOptFetchDump(stdout, root);
	}
    }
    if (root != (fetchctl_t *)0) {
	printf("Botch: expected no fetch lists after all requests deleted, got ...\n");
	__pmOptFetchDump(stdout, root);
	root = (fetchctl_t *)0;
	numfail++;
    }

    printf("Passed %d of %d deletion tests\n", numreq - numfail, numreq);

    /* add them all back again */
    for (i = 0; i < numreq; i++) {
	req[i].r_desc = &desc[i];
	__pmOptFetchAdd(&root, &req[i]);
    }
    numfetch = 0;
    totcost = 0;
    for (fp = root; fp != (fetchctl_t *)0; fp = fp->f_next) {
	totcost += fp->f_cost;
	numfetch++;
    }
    printf("Before re-arrangement no. fetches %d, total cost %d\n", numfetch, totcost);

    /* and re-arrange, a few times */
    for (i = 0; i < 10; i++) {
	__pmOptFetchRedo(&root);
	if (pmDebugOptions.optfetch) {
	    fprintf(stdout, "\nNow try a redo ...\n");
	    __pmOptFetchDump(stdout, root);
	}
	numfetch = 0;
	totcost = 0;
	for (fp = root; fp != (fetchctl_t *)0; fp = fp->f_next) {
	    totcost += fp->f_cost;
	    numfetch++;
	}
	printf("After re-arrangement no. fetches %d, total cost %d\n", numfetch, totcost);
    }

    exit(0);
}
