/*
 * procmemstat - sample, simple PMAPI client to report your own memory
 * usage
 *
 * Copyright (c) 2013-2014 Red Hat.
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmnsmap.h"

static const char	*scale = "kbytes";

static void
get_sample(void)
{
    int			first = 1;
    static pmResult	*rp;
    static int		numpmid;
    static pmID		*pmidlist;
    static pmDesc	*desclist;
    static pmUnits	scaleunits;
    static double	scalemult;
    int			pid;
    pmAtomValue		tmp;
    pmAtomValue		atom;
    int			sts;
    int			i;

    if (first) {
	sts = pmParseUnitsStr(scale, &scaleunits, &scalemult);
	if (sts < 0) {
	    fprintf(stderr, "%s: unit/scale parse error\n", pmProgname, osstrerror());
	    exit(1);
	}

	numpmid = sizeof(metrics) / sizeof(char *);
	if ((pmidlist = (pmID *)malloc(numpmid * sizeof(pmidlist[0]))) == NULL) {
	    fprintf(stderr, "%s: get_sample: malloc: %s\n", pmProgname, osstrerror());
	    exit(1);
	}
	if ((desclist = (pmDesc *)malloc(numpmid * sizeof(desclist[0]))) == NULL) {
	    fprintf(stderr, "%s: get_sample: malloc: %s\n", pmProgname, osstrerror());
	    exit(1);
	}
	if ((sts = pmLookupName(numpmid, metrics, pmidlist)) < 0) {
	    printf("%s: pmLookupName: %s\n", pmProgname, pmErrStr(sts));
	    for (i = 0; i < numpmid; i++) {
		if (pmidlist[i] == PM_ID_NULL)
		    fprintf(stderr, "%s: metric \"%s\" not in name space\n", pmProgname, metrics[i]);
	    }
	    exit(1);
	}
	for (i = 0; i < numpmid; i++) {
	    if ((sts = pmLookupDesc(pmidlist[i], &desclist[i])) < 0) {
		fprintf(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
		    pmProgname, metrics[i], pmIDStr(pmidlist[i]), pmErrStr(sts));
		exit(1);
	    }
	}
	/*
	 * All metrics we care about share the same instance domain,
	 * and the instance of interest is _my_ PID
	 */
	pmDelProfile(desclist[0].indom, 0, NULL);	/* all off */
	pid = (int)getpid();
	pmAddProfile(desclist[0].indom, 1, &pid);

	first = 0;
    }

    /* fetch the current metrics */
    if ((sts = pmFetch(numpmid, pmidlist, &rp)) < 0) {
	fprintf(stderr, "%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    printf("memory metrics for pid %" FMT_PID " (sizes in %s)\n", pid, scale);
    for (i = 0; i < numpmid; i++) {
	/* process metrics in turn */
	pmExtractValue(rp->vset[i]->valfmt, rp->vset[i]->vlist,
		       desclist[i].type, &tmp, PM_TYPE_32);
	sts = pmConvScale(PM_TYPE_32, &tmp, &desclist[i].units,
			  &atom, &scaleunits);
	if (sts == 0)
	    printf("%8d %s\n", (int)(atom.l * scalemult), metrics[i]);
	else
	    printf("???????? %s\n", metrics[i]);
    }
}

pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    {"", 1, 'u', "units", "rescale units (default kbytes)"},
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

pmOptions opts = {
    .short_options = "D:u:?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    int			sts;
    char		*p;
    char		*q;
    int			c;

    setlinebuf(stdout);
    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	if (c == 'u')
	    scale = opts.optarg;
    }

    if (opts.errors || opts.optind < argc - 1) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, "local:")) < 0) {
	fprintf(stderr, "%s: Cannot connect to PMCD on host \"local:\": %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }

    get_sample();

#define ARRAY 1*1024*1024
    p = (char *)malloc(ARRAY);
    for (q = p; q < &p[ARRAY]; q += 1024)
	*q = '\0';
    printf("\nAfter malloc ...\n");
    get_sample();

    exit(0);
}
