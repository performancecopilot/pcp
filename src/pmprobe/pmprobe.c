/*
 * pmprobe - light-weight pminfo for configuring monitor apps
 *
 * Copyright (c) 2013-2014 Red Hat.
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
#include "impl.h"

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
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "force", 0, 'f', 0, "report all pmGetIndom or pmGetInDomArchive instances" },
    { "external", 0, 'I', 0, "list external instance names" },
    { "internal", 0, 'i', 0, "list internal instance numbers" },
    { "verbose", 0, 'V', 0, "report PDU operations (verbose)" },
    { "values", 0, 'v', 0, "list metric values" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ,
    .short_options = "a:D:fh:IiK:Ln:O:VvZ:z?",
    .long_options = longopts,
    .short_usage = "[options] [metricname ...]",
};

static void
dometric(const char *name)
{
    if (numpmid >= listsize) {
	size_t size;

	listsize = listsize == 0 ? 16 : listsize * 2;
	size = listsize * sizeof(pmidlist[0]);
	if ((pmidlist = (pmID *)realloc(pmidlist, size)) == NULL)
	    __pmNoMem("realloc pmidlist", size, PM_FATAL_ERR);
	size = listsize * sizeof(namelist[0]);
	if ((namelist = (char **)realloc(namelist, size)) == NULL)
	    __pmNoMem("realloc namelist", size, PM_FATAL_ERR);
    }

    namelist[numpmid]= strdup(name);
    if (namelist[numpmid] == NULL)
	__pmNoMem("strdup name", strlen(name), PM_FATAL_ERR);

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
    int		numinst;
    int		fflag = 0;		/* -f pmGetIndom or pmGetIndomArchive for instances */
    int		iflag = 0;		/* -i for instance numbers */
    int		Iflag = 0;		/* -I for instance names */
    int		vflag = 0;		/* -v for values */
    int		Vflag = 0;		/* -V for verbose */
    char	*source;
    pmResult	*result;
    pmValueSet	*vsp;
    pmDesc	desc;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'f':	/* pmGetIndom or pmGetInDomArchive for instances with -i or -I */
	    fflag++;
	    break;

	case 'i':	/* report internal instance numbers */
	    if (vflag) {
		pmprintf("%s: at most one of -i and -v allowed\n", pmProgname);
		opts.errors++;
	    }
	    iflag++;
	    break;

	case 'I':	/* report external instance names */
	    if (vflag) {
		pmprintf("%s: at most one of -I and -v allowed\n", pmProgname);
		opts.errors++;
	    }
	    Iflag++;
	    break;

	case 'V':	/* verbose */
	    Vflag++;
	    break;

	case 'v':	/* cheap values */
	    if (iflag || Iflag) {
		pmprintf("%s: at most one of -v and (-i or -I) allowed\n", pmProgname);
		opts.errors++;
	    }
	    vflag++;
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
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
		pmProgname, source, pmErrStr(sts));
	else if (opts.context == PM_CONTEXT_LOCAL)
	    fprintf(stderr, "%s: Cannot make standalone connection on localhost: %s\n",
		    pmProgname, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, source, pmErrStr(sts));
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
    else {
	for (i = opts.optind; i < argc; i++) {
	    sts = pmTraversePMNS(argv[i], dometric);
	    if (sts < 0)
		printf("%s %d %s\n", argv[i], sts, pmErrStr(sts));
	}
    }

    /* Lookup names.
     * Cull out names that were unsuccessfully looked up.
     * However, it is unlikely to fail because names come from a traverse PMNS.
     */
    if (numpmid > 0 && (sts = pmLookupName(numpmid, namelist, pmidlist)) < 0) {
	for (i = j = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL) {
		printf("%s %d %s\n", namelist[i], sts, pmErrStr(sts));
		free(namelist[i]);
	    }
	    else {
		/* assert(j <= i); */
		pmidlist[j] = pmidlist[i];
		namelist[j] = namelist[i];
		j++;
	    }	
	}
	numpmid = j;
    }

    fetch_sts = 0;
    for (i = 0; i < numpmid; i++) {
	printf("%s ", namelist[i]);

	if (iflag || Iflag || vflag) {
	    if ((sts = pmLookupDesc(pmidlist[i], &desc)) < 0) {
		printf("%d %s (pmLookupDesc)\n", sts, pmErrStr(sts));
		continue;
	    }
	}

	if (fflag && (iflag || Iflag)) {
	    /*
	     * must be -i or -I with -f ... don't even fetch a result
	     * with pmFetch, just go straight to the instance domain with
	     * pmGetInDom or pmGetInDomArchive
	     */
	    if (desc.indom == PM_INDOM_NULL) {
		printf("1 PM_IN_NULL");
		if ( iflag && Iflag )
		    printf(" PM_IN_NULL");
	    }
	    else {
		if ((numinst = lookup(desc.indom)) < 0) {
		    printf("%d %s (pmGetInDom)", numinst, pmErrStr(numinst));
		}
		else {
		    int		j;
		    printf("%d", numinst);
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
	     * get them all at once
	     */
	    if ((sts = pmSetMode(PM_MODE_FORW, &opts.start, 0)) < 0) {
		printf("%d %s (pmSetMode)\n", sts, pmErrStr(sts));
		continue;
	    }
	    fetch_sts = pmFetch(1, &pmidlist[i], &result);
	}
	else {
	    if (i == 0)
		fetch_sts = pmFetch(numpmid, pmidlist, &result);
	}

	if (fetch_sts < 0) {
	    printf("%d %s (pmFetch)", fetch_sts, pmErrStr(fetch_sts));
	}
	else {
	    if (opts.context == PM_CONTEXT_ARCHIVE)
	    	vsp = result->vset[0];
	    else
	    	vsp = result->vset[i];

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
	}
	putchar('\n');
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
