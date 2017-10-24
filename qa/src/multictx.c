/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2016 Ken McDonell.  All Rights Reserved.
 *
 * Exercise multiple contexts ... aim to reproduce
 * http://oss.sgi.com/bugzilla/show_bug.cgi?id=1158
 *
 * One metric name per argument.
 *
 * PMAPI loop repeated for each metric.
 * - pmLookupName(all names)
 * - pmLookupDesc for one metric
 * - pmNameID for one metric
 * - pmGetInDom for one metric
 * - pmNameInDom for one metric
 * - pmLookupInDom for one metric
 * - pmLookupText for one metric
 * - pmTraversePMNS for parent of one metric
 * - pmGetChildrenStatus for parent of one metric
 * - pmFetch for one metric
 *
 * Each context starts with a different metric, and at a different point
 * in the loop.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <string.h>

typedef struct {
    char	*name;
    pmID	pmid;
    pmDesc	desc;
    int		ctx;
    int		ctx_bad;	/* after IPC or TIMEOUT & before Reconnect */
    int		op;		/* next operation */
    int		midx;		/* next metric */
    char	*parent;	/* parent name in PMNS */
    int		inst;		/* middle instance, assumes indom is not dynamic */
    char	*instname;	/* middle instance name */
} ctl_t;

#define OP_LOOKUPNAME	0
#define OP_GETDESC	1
#define OP_NAMEID	2
#define OP_GETINDOM	3
#define OP_NAMEINDOM	4
#define OP_LOOKUPINDOM	5
#define OP_LOOKUPTEXT	6
#define OP_TRAVERSE	7
#define OP_GETCHILDREN	8
#define OP_FETCH	9
#define NUMOP		10

int	nummetrics;

static void
dometric(const char *name)
{
    nummetrics++;
}

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    char	*host = NULL;			/* pander to gcc */
    char	local[MAXHOSTNAMELEN];
    int		samples = 10;
    char	*endnum;
    ctl_t	*ctl;
    int		numctl;
    pmID	pmid;
    char	*name;
    pmDesc	desc;
    pmResult	*rp;
    int		iter;
    char	**offspring;
    int		*status;
    char	*p;
    int		*instlist;
    char	**namelist;

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:h:Ls:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
#ifdef BUILD_STANDALONE
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
#else
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
#endif
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
#ifdef BUILD_STANDALONE
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
#else
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
#endif
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

#ifdef BUILD_STANDALONE
	case 'L':	/* LOCAL, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmProgname);
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
	    putenv("PMDA_LOCAL_PROC=");		/* if proc PMDA needed */
	    putenv("PMDA_LOCAL_SAMPLE=");	/* if sampledso PMDA needed */
	    break;
#endif

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    numctl = argc - optind;
    if (numctl < 1)
	errflag++;

    if (errflag) {
	fprintf(stderr, 
"Usage: %s [options] metric [...]\n\
\n\
Options:\n\
  -a archive     metrics source is a PCP log archive\n\
  -h host        metrics source is PMCD on host\n"
#ifdef BUILD_STANDALONE
"  -L             use local context instead of PMCD\n"
#endif
"  -s samples     terminate after this many samples [default 10]\n",
                pmProgname);
        exit(1);
    }

    if (type == 0) {
	type = PM_CONTEXT_HOST;
	(void)gethostname(local, MAXHOSTNAMELEN);
	local[MAXHOSTNAMELEN-1] = '\0';
	host = local;
    }

    ctl = (ctl_t *)malloc(numctl * sizeof(ctl[0]));
    if (ctl == NULL) {
	__pmNoMem("ctl", numctl*sizeof(ctl[0]), PM_FATAL_ERR);
	/* NOTREACHED */
    }

    i = 0;
    /* non-flag args are argv[optind] ... argv[argc-1] */
    while (optind < argc) {

	if ((sts = pmNewContext(type, host)) < 0) {
	    if (type == PM_CONTEXT_HOST)
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmProgname, host, pmErrStr(sts));
	    else
		fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmProgname, host, pmErrStr(sts));
	    exit(1);
	}
	ctl[i].ctx = sts;
	ctl[i].ctx_bad = 0;
	ctl[i].name = argv[optind];
	sts = pmLookupName(1, &ctl[i].name, &ctl[i].pmid);
	if (sts < 0) {
	    fprintf(stderr, "pmLookupName(%s): %s\n", ctl[i].name, pmErrStr(sts));
	    exit(1);
	}
	sts = pmLookupDesc(ctl[i].pmid, &ctl[i].desc);
	if (sts < 0) {
	    fprintf(stderr, "pmLookupDesc(%s): %s\n", ctl[i].name, pmErrStr(sts));
	    exit(1);
	}
	p = strrchr(ctl[i].name, '.');
	if (p == NULL) {
	    ctl[i].parent = "";
	}
	else {
	    c = *p;
	    *p = '\0';
	    ctl[i].parent = strdup(ctl[i].name);
	    *p = c;
	}
	sts = pmGetInDom(ctl[i].desc.indom, &instlist, &namelist);
	if (sts <= 0) {
	    /* errors and zero sized instance domains are the same */
	    ctl[i].inst = PM_IN_NULL;
	    ctl[i].instname = "";
	}
	else {
	    int		j = sts / 2;
	    ctl[i].inst = instlist[j];
	    ctl[i].instname = strdup(namelist[j]);
	    free(instlist);
	    free(namelist);
	}
	ctl[i].op = i;
	ctl[i].midx = i;
	i++;
	optind++;
    }

    for (iter = 0; iter < samples; iter++) {
	/* work forwards through the control structures */
	i = (iter % numctl);

	if (ctl[i].ctx_bad) {
	    sts = pmReconnectContext(ctl[i].ctx);
	    if (sts >= 0) {
		fprintf(stderr, "iter %d: pmReconnectContext(%d) -> %d OK\n", iter, ctl[i].ctx, sts);
		ctl[i].ctx = sts;
		ctl[i].ctx_bad = 0;
	    }
	    else {
		fprintf(stderr, "iter %d: pmReconnectContext(%d) failed: %s\n", iter, ctl[i].ctx, pmErrStr(sts));
		continue;
	    }
	}

	sts = pmUseContext(ctl[i].ctx);
	if (sts < 0) {
	    fprintf(stderr, "iter %d: pmUseContext(%d): %s\n", iter, ctl[i].ctx, pmErrStr(sts));
	    exit(1);
	}

	switch (ctl[i].op) {

	case OP_LOOKUPNAME:
	    fprintf(stderr, "iter %d: ctx %d: pmLookupName metric: %s\n", iter, ctl[i].ctx, ctl[i].name);
	    sts = pmLookupName(1, &ctl[i].name, &pmid);
	    if (sts < 0) {
		fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		    ctl[i].ctx_bad = 1;
	    }
	    else {
		if (pmid != ctl[i].pmid) {
		    fprintf(stderr, "Error: returned name %s", pmIDStr(pmid));
		    fprintf(stderr, " expecting %s\n", pmIDStr(ctl[i].pmid));
		}
	    }
	    break;

	case OP_GETDESC:
	    fprintf(stderr, "iter %d: ctx %d: pmLookupDesc metric: %s\n", iter, ctl[i].ctx, ctl[i].name);
	    sts = pmLookupDesc(ctl[i].pmid, &desc);
	    if (sts < 0) {
		fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		    ctl[i].ctx_bad = 1;
	    }
	    else {
		if (desc.pmid != ctl[i].pmid) {
		    fprintf(stderr, "Error: returned pmid %s", pmIDStr(desc.pmid));
		    fprintf(stderr, " expecting %s\n", pmIDStr(ctl[i].pmid));
		}
		if (desc.pmid != ctl[i].desc.pmid) {
		    fprintf(stderr, "Error: returned desc.pmid %s", pmIDStr(desc.pmid));
		    fprintf(stderr, " expecting %s\n", pmIDStr(ctl[i].desc.pmid));
		}
		if (desc.type != ctl[i].desc.type) {
		    fprintf(stderr, "Error: returned desc.type %s", pmTypeStr(desc.type));
		    fprintf(stderr, " expecting %s\n", pmTypeStr(ctl[i].desc.type));
		}
		if (desc.indom != ctl[i].desc.indom) {
		    fprintf(stderr, "Error: returned desc.indom %s", pmInDomStr(desc.indom));
		    fprintf(stderr, " expecting %s\n", pmInDomStr(ctl[i].desc.indom));
		}
		if (desc.sem != ctl[i].desc.sem) {
		    fprintf(stderr, "Error: returned desc.sem %s", pmSemStr(desc.sem));
		    fprintf(stderr, " expecting %s\n", pmSemStr(ctl[i].desc.sem));
		}
		if (desc.units.scaleCount != ctl[i].desc.units.scaleCount ||
		    desc.units.scaleTime != ctl[i].desc.units.scaleTime ||
		    desc.units.scaleSpace != ctl[i].desc.units.scaleSpace ||
		    desc.units.dimCount != ctl[i].desc.units.dimCount ||
		    desc.units.dimTime != ctl[i].desc.units.dimTime ||
		    desc.units.dimSpace != ctl[i].desc.units.dimSpace) {
		    fprintf(stderr, "Error: returned desc.units %s", pmUnitsStr(&desc.units));
		    fprintf(stderr, " expecting %s\n", pmUnitsStr(&ctl[i].desc.units));
		}
	    }
	    break;

	case OP_NAMEID:
	    fprintf(stderr, "iter %d: ctx %d: pmNameID metric: %s (aka %s)\n", iter, ctl[i].ctx, pmIDStr(ctl[i].pmid), ctl[i].name);
	    sts = pmNameID(ctl[i].pmid, &name);
	    if (sts < 0) {
		fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		    ctl[i].ctx_bad = 1;
	    }
	    else {
		if (strcmp(ctl[i].name, name) != 0) {
		    fprintf(stderr, "Error: returned name %s", name);
		    fprintf(stderr, " expecting %s\n", ctl[i].name);
		}
		free(name);
	    }
	    break;

	case OP_GETINDOM:
	    fprintf(stderr, "iter %d: ctx %d: pmGetInDom metric: %s (aka %s)\n", iter, ctl[i].ctx, pmIDStr(ctl[i].pmid), ctl[i].name);
	    sts = pmGetInDom(ctl[i].desc.indom, &instlist, &namelist);
	    if (sts < 0) {
		fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		    ctl[i].ctx_bad = 1;
	    }
	    else {
		if (ctl[i].inst != PM_IN_NULL) {
		    int	j = sts / 2;
		    if (instlist[j] != ctl[i].inst) {
			fprintf(stderr, "Error: returned inst[%d] %d", j, instlist[j]);
			fprintf(stderr, " expecting %d\n", ctl[i].inst);
		    }
		    if (strcmp(namelist[j], ctl[i].instname) != 0) {
			fprintf(stderr, "Error: returned instname[%d] \"%s\"", j, namelist[j]);
			fprintf(stderr, " expecting \"%s\"\n", ctl[i].instname);
		    }
		}
		free(instlist);
		free(namelist);
	    }
	    break;

	case OP_NAMEINDOM:
	    fprintf(stderr, "iter %d: ctx %d: pmNameInDom metric: %s (aka %s)\n", iter, ctl[i].ctx, pmIDStr(ctl[i].pmid), ctl[i].name);
	    sts = pmNameInDom(ctl[i].desc.indom, ctl[i].inst, &p);
	    if (sts < 0) {
		fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		    ctl[i].ctx_bad = 1;
	    }
	    else {
		if (ctl[i].inst != PM_IN_NULL) {
		    if (strcmp(p, ctl[i].instname) != 0) {
			fprintf(stderr, "Error: returned instname \"%s\"", p);
			fprintf(stderr, " expecting \"%s\"\n", ctl[i].instname);
		    }
		}
		free(p);
	    }
	    break;

	case OP_LOOKUPINDOM:
	    fprintf(stderr, "iter %d: ctx %d: pmLookupInDom metric: %s (aka %s)\n", iter, ctl[i].ctx, pmIDStr(ctl[i].pmid), ctl[i].name);
	    sts = pmLookupInDom(ctl[i].desc.indom, ctl[i].instname);
	    if (sts < 0) {
		fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		    ctl[i].ctx_bad = 1;
	    }
	    else {
		if (ctl[i].inst != PM_IN_NULL) {
		    if (sts != ctl[i].inst) {
			fprintf(stderr, "Error: returned inst %d", sts);
			fprintf(stderr, " expecting %d\n", ctl[i].inst);
		    }
		}
	    }
	    break;

	case OP_LOOKUPTEXT:
	    fprintf(stderr, "iter %d: ctx %d: pmLookupText metric: %s (aka %s)\n", iter, ctl[i].ctx, pmIDStr(ctl[i].pmid), ctl[i].name);
	    sts = pmLookupText(ctl[i].pmid, PM_TEXT_ONELINE, &p);
	    if (sts < 0) {
		fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		    ctl[i].ctx_bad = 1;
	    }
	    else {
		free(p);
	    }
	    break;

	case OP_TRAVERSE:
	    fprintf(stderr, "iter %d: ctx %d: pmTraversePMNS metric: %s\n", iter, ctl[i].ctx, ctl[i].parent);
	    nummetrics = 0;
	    sts = pmTraversePMNS(ctl[i].parent, dometric);
	    if (sts <= 0) {
		fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		    ctl[i].ctx_bad = 1;
	    }
	    else {
		if (nummetrics == 0) {
		    fprintf(stderr, "Error: returned %d metrics", nummetrics);
		    fprintf(stderr, " expecting something > 0\n");
		}
	    }
	    break;

	case OP_GETCHILDREN:
	    fprintf(stderr, "iter %d: ctx %d: pmGetChildrenStatus metric: %s\n", iter, ctl[i].ctx, ctl[i].parent);
	    sts = pmGetChildrenStatus(ctl[i].parent, &offspring, &status);
	    if (sts <= 0) {
		fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		    ctl[i].ctx_bad = 1;
	    }
	    else {
		/* anything > 0 is OK */
		free(offspring);
		free(status);
	    }
	    break;

	case OP_FETCH:
	    fprintf(stderr, "iter %d: ctx %d: pmFetch metric: %s (aka %s)\n", iter, ctl[i].ctx, pmIDStr(ctl[i].pmid), ctl[i].name);
	    sts = pmFetch(1, &ctl[i].pmid, &rp);
	    if (sts < 0) {
		fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		    ctl[i].ctx_bad = 1;
	    }
	    else {
		if (rp->numpmid != 1) {
		    fprintf(stderr, "Error: returned numpmid %d", rp->numpmid);
		    fprintf(stderr, " expecting %d\n", 1);
		}
		if (rp->vset[0]->pmid != ctl[i].pmid) {
		    fprintf(stderr, "Error: returned vset[0]->pmid %s", pmIDStr(rp->vset[0]->pmid));
		    fprintf(stderr, " expecting %s\n", pmIDStr(ctl[i].pmid));
		}
		pmFreeResult(rp);
	    }
	    break;

	}

	/*
	 * work forwards (even context) or backwards (odd context)
	 * through the PMAPI operations
	 */
	if (i % 2 == 0) {
	    ctl[i].op++;
	    if (ctl[i].op == NUMOP)
		ctl[i].op = 0;
	}
	else {
	    ctl[i].op--;
	    if (ctl[i].op < 0)
		ctl[i].op = NUMOP - 1;
	}
	/* work backwards through  the metrics */
	ctl[i].midx--;
	if (ctl[i].midx < 0)
	    ctl[i].midx = numctl - 1;
    }

    return 0;
}
