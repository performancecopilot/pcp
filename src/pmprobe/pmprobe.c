/*
 * pmprobe - light-weight pminfo for configuring monitor apps
 *
 * Copyright (c) 2013-2017 Red Hat.
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <unistd.h>
#include "pmapi.h"
#include "libpcp.h"

static char	**namelist;
static pmID	*pmidlist;
static int	listsize;
static int	numpmid;
static int	*instlist;
static char	**instnamelist;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_ARCHIVE,
    PMOPT_DEBUG,
    PMOPT_HOST,
    PMOPT_CONTAINER,
    PMOPT_DERIVED,
    PMOPT_LOCALPMDA,
    PMOPT_SPECLOCAL,
    PMOPT_NAMESPACE,
    PMOPT_ORIGIN,
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    { "version", 0, 'd', 0, "display version number and exit" },
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Protocol options"),
    { "batch",    1, 'b', "N", "fetch at most N metrics at a time [128]" },
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "force", 0, 'f', 0, "report all pmGetIndom or pmGetInDomArchive instances" },
    { "faster", 0, 'F', 0, "assume given metric names are PMNS leaf nodes" },
    { "external", 0, 'I', 0, "list external instance names" },
    { "internal", 0, 'i', 0, "list internal instance numbers" },
    { "verbose", 0, 'V', 0, "report PDU operations (verbose)" },
    { "values", 0, 'v', 0, "list metric values" },
    PMAPI_OPTIONS_END
};

static int
overrides(int opt, pmOptions *opts)
{
    if (opt == 'V')
	return 1;	/* we've claimed this, inform pmGetOptions */
    return 0;
}

static pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ,
    .short_options = "a:b:D:efh:IiK:Ln:FO:VvZ:z?",
    .long_options = longopts,
    .short_usage = "[options] [metricname ...]",
    .override = overrides,
};

static void
dometric(const char *name)
{
    if (numpmid >= listsize) {
	size_t size;

	listsize = listsize == 0 ? 16 : listsize * 2;
	size = listsize * sizeof(pmidlist[0]);
	if ((pmidlist = (pmID *)realloc(pmidlist, size)) == NULL)
	    pmNoMem("realloc pmidlist", size, PM_FATAL_ERR);
	size = listsize * sizeof(namelist[0]);
	if ((namelist = (char **)realloc(namelist, size)) == NULL)
	    pmNoMem("realloc namelist", size, PM_FATAL_ERR);
    }

    namelist[numpmid]= strdup(name);
    if (namelist[numpmid] == NULL)
	pmNoMem("strdup name", strlen(name), PM_FATAL_ERR);

    numpmid++;
}

static int
lookup(pmInDom indom)
{
    static pmInDom	last = PM_INDOM_NULL;
    static int		numinst;

    if (indom != last) {
	if (numinst > 0) {
	    free(instlist);
	    free(instnamelist);
	}
	if (opts.context == PM_CONTEXT_ARCHIVE)
	    numinst = pmGetInDomArchive(indom, &instlist, &instnamelist);
	else
	    numinst = pmGetInDom(indom, &instlist, &instnamelist);
	last = indom;
    }
    return numinst;
}

int
main(int argc, char **argv)
{
    int		c, i, j, sts;
    int		fetch_sts;
    int		fetched;
    int		numinst;
    int		fflag = 0;		/* -f pmGetIndom or pmGetIndomArchive for instances */
    int		Fflag = 0;		/* -F for fast leaf names access */
    int		iflag = 0;		/* -i for instance numbers */
    int		Iflag = 0;		/* -I for instance names */
    int		vflag = 0;		/* -v for values */
    int		Vflag = 0;		/* -V for verbose */
    int		batchsize = 128;	/* -b batchsize for pmFetch in live mode only */
    char	*source;
    pmResult	*result;
    pmValueSet	*vsp;
    pmDesc	desc;
    int		batch;
    int		batchidx;
    int		batchbytes;
    char	*endnum;
    int		ceiling = PDU_CHUNK * 64;
    int		newnumpmid;
    int		b;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'b':           /* batchsize */
	    batchsize = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0') {
		pmprintf("%s: -b requires numeric argument\n", pmGetProgname());
		opts.errors++;
	    }
	    break;

	case 'f':	/* pmGetIndom or pmGetInDomArchive for instances with -i or -I */
	    fflag++;
	    break;

	case 'i':	/* report internal instance numbers */
	    if (vflag) {
		pmprintf("%s: at most one of -i and -v allowed\n", pmGetProgname());
		opts.errors++;
	    }
	    iflag++;
	    break;

	case 'I':	/* report external instance names */
	    if (vflag) {
		pmprintf("%s: at most one of -I and -v allowed\n", pmGetProgname());
		opts.errors++;
	    }
	    Iflag++;
	    break;

	case 'F':	/* optimised access to leaf names */
	    Fflag++;
	    break;

	case 'd':	/* version (d'oh - 'V' already in use) */
	    pmprintf("%s version %s\n", pmGetProgname(), PCP_VERSION);
	    opts.flags |= PM_OPTFLAG_EXIT;
	    break;

	case 'V':	/* verbose */
	    Vflag++;
	    break;

	case 'v':	/* cheap values */
	    if (iflag || Iflag) {
		pmprintf("%s: at most one of -v and (-i or -I) allowed\n", pmGetProgname());
		opts.errors++;
	    }
	    vflag++;
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	sts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(sts);
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

    if ((sts = c = pmNewContext(opts.context, source)) < 0) {
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
    if (opts.context == PM_CONTEXT_ARCHIVE) {
	if (pmGetContextOptions(c, &opts)) {
	    pmflush();
	    exit(1);
	}
    }

    if (opts.optind >= argc)
	pmTraversePMNS("", dometric);
    else if (Fflag) {
	for (i = opts.optind; i < argc; i++)
	    dometric(argv[i]);
    }
    else {
	for (i = opts.optind; i < argc; i++) {
	    sts = pmTraversePMNS(argv[i], dometric);
	    if (sts < 0)
		printf("%s %d %s\n", argv[i], sts, pmErrStr(sts));
	}
    }

    /* Lookup names, in batches if necessary to avoid the 64k PDU size ceiling.
     * Cull out names that were unsuccessfully looked up.
     * However, it is unlikely to fail because names come from a traverse PMNS.
     */
    for (newnumpmid = batchidx = 0; batchidx < numpmid;) {
	/* figure out batch size (normally all numpmid for small requests) */ 
	for (b=0, batchbytes=0; b + batchidx < numpmid; b++) {
	    int len = strlen(namelist[batchidx + b]) + 32; /* approx PDU len, per name */
	    if ( b > batchsize || len + batchbytes >= ceiling) {
		/* do not exceed requested batch size, nor the PDU ceiling */
		b--; /* back off one */
	    	break;
	    }
	    batchbytes += len;
	}
	if (pmDebugOptions.appl0) {
	    pmprintf("%s: name lookup, batchidx=%d batchbytes=%d b=%d\n",
		pmGetProgname(), batchidx, batchbytes, b);
	}
	if (b > 0 && (sts = pmLookupName(b, namelist + batchidx, pmidlist + batchidx)) < 0) {
	    for (i = j = 0; i < b; i++) {
		if (pmidlist[batchidx + i] == PM_ID_NULL) {
		    printf("%s %d %s\n", namelist[batchidx + i], sts, pmErrStr(sts));
		    free(namelist[batchidx + i]);
		}
		else {
		    /* assert(j <= i); */
		    pmidlist[batchidx + j] = pmidlist[batchidx + i];
		    namelist[batchidx + j] = namelist[batchidx + i];
		    j++;
		}
	    }
	    newnumpmid += j;
	}
	else
	    newnumpmid += b;
	batchidx += b;
    }
    numpmid = newnumpmid;

    fetch_sts = fetched = 0;
    for (i = 0; i < numpmid; i += batch) {
	batch = 1;
	if (fflag && (iflag || Iflag)) {
	    /*
	     * must be -i or -I with -f ... don't even fetch a result
	     * with pmFetch, just go straight to the instance domain with
	     * pmGetInDom or pmGetInDomArchive
	     */
	    if ((sts = pmLookupDesc(pmidlist[i], &desc)) < 0) {
		printf("%s %d %s (pmLookupDesc)\n", namelist[i], sts, pmErrStr(sts));
		continue;
	    }
	    if (desc.indom == PM_INDOM_NULL) {
		printf("%s 1 PM_IN_NULL", namelist[i]);
		if ( iflag && Iflag )
		    printf(" PM_IN_NULL");
	    }
	    else {
		if ((numinst = lookup(desc.indom)) < 0) {
		    printf("%s %d %s (pmGetInDom)", namelist[i], numinst, pmErrStr(numinst));
		}
		else {
		    int		j;
		    printf("%s %d", namelist[i], numinst);
		    for (j = 0; j < numinst; j++) {
			if (iflag)
			    printf(" ?%d", instlist[j]);
			if (Iflag)
			    printf(" \"%s\"", instnamelist[j]);
		    }
		}
	    }
	    putchar('\n');
	    continue;
	}

	if (opts.context == PM_CONTEXT_ARCHIVE) {
	    /*
	     * merics from archives are fetched one at a time, otherwise
	     * get them in batches of at most batchsize.
	     */
	    if ((sts = pmSetMode(PM_MODE_FORW, &opts.origin, 0)) < 0) {
		printf("%d %s (pmSetMode)\n", sts, pmErrStr(sts));
		continue;
	    }
	    fetch_sts = pmFetch(1, &pmidlist[fetched], &result);
	    batch = 1;
	}
	else {
	    int remaining = numpmid - fetched;

	    batch = (remaining > batchsize) ? batchsize : remaining;
	    fetch_sts = pmFetch(batch, &pmidlist[fetched], &result);
	    if (pmDebugOptions.appl0) {
		pmprintf("%s: batch fetch, numpmid=%d i=%d fetched=%d remaining=%d batch=%d\n",
		    pmGetProgname(), numpmid, i, fetched, remaining, batch);
	    }
	}
	if (fetch_sts < 0) {
	    printf("%s %d %s (pmFetch)\n", namelist[fetched], fetch_sts, pmErrStr(fetch_sts));
	    fetched += batch;
	    continue;
	}

	/*
	 * loop and report for each value set in the batched result
	 */
	for (b=0; b < batch; b++) {
	    printf("%s ", namelist[fetched + b]);
	    vsp = result->vset[b];

	    if (iflag || Iflag || vflag) {
		/* get the desc for this metric valueset */
		if ((sts = pmLookupDesc(pmidlist[fetched + b], &desc)) < 0) {
		    printf("%d %s (pmLookupDesc)\n", sts, pmErrStr(sts));
		    continue;
		}
	    }

	    if (vsp->numval < 0) {
		printf("%d %s", vsp->numval, pmErrStr(vsp->numval));
	    }
	    else if (vsp->numval == 0) {
		printf("0");
		;
	    }
	    else if (vsp->numval > 0) {
		printf("%d", vsp->numval);
		if (vflag) {
		    for (j = 0; j < vsp->numval; j++) {
			pmValue	*vp = &vsp->vlist[j];
			putchar(' ');
			pmPrintValue(stdout, vsp->valfmt, desc.type, vp, 1);
		    }
		}
		if (iflag || Iflag) {
		    /*
		     * must be without -f
		     * get instance domain for reporting to minimize PDU
		     * round trips ... state should be the same as of the
		     * pmResult, so each instance in the pmResult should be
		     * found by pmGetInDom or pmGetInDomArchive
		     */
		    if (desc.indom == PM_INDOM_NULL) {
			printf(" PM_IN_NULL");
			if ( iflag && Iflag )
			    printf(" PM_IN_NULL");
			putchar ('\n');
			continue;
		    }
		    if ((numinst = lookup(desc.indom)) < 0) {
			printf("%d %s (pmGetInDom)\n", numinst, pmErrStr(numinst));
			continue;
		    }
		    for (j = 0; j < vsp->numval; j++) {
			pmValue		*vp = &vsp->vlist[j];
			if (iflag)
			    printf(" ?%d", vp->inst);
			if (Iflag) {
			    int		k;
			    for (k = 0; k < numinst; k++) {
				if (instlist[k] == vp->inst)
				    break;
			    }
			    if (k < numinst)
				printf(" \"%s\"", instnamelist[k]);
			    else
				printf(" ?%d", vp->inst);
			}
		    }
		}
	    }
	    putchar('\n');
	}

	fetched += batch;
    }

    if (Vflag) {
	printf("PDUs send");
	for (i = j = 0; i < PDU_MAX; i++) {
	    printf(" %3d", __pmPDUCntOut[i]);
	    j += __pmPDUCntOut[i];
	}
	printf("\nTotal: %d\n", j);

	printf("PDUs recv");
	for (i = j = 0; i < PDU_MAX; i++) {
	    printf(" %3d",__pmPDUCntIn[i]);
	    j += __pmPDUCntIn[i];
	}
	printf("\nTotal: %d\n", j);
    }

    exit(0);
}
