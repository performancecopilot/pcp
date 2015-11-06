/*
 * pmstore [-h hostname ] [-i inst[,inst...]] [-n pmnsfile ] metric value
 *
 * Copyright (c) 2013-2015 Red Hat.
 * Copyright (c) 1995,2004-2008 Silicon Graphics, Inc.  All Rights Reserved.
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

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_DEBUG,
    PMOPT_HOST,
    PMOPT_LOCALPMDA,
    PMOPT_SPECLOCAL,
    PMOPT_NAMESPACE,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Value options"),
    { "force", 0, 'f', 0, "store the value even if there is no current value set" },
    { "insts", 1, 'i', "INSTS", "restrict store to comma-separated list of instances" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_POSIX,
    .short_options = "D:fh:K:Li:n:?",
    .long_options = longopts,
    .short_usage = "[options] metricname value",
};

static void
mkAtom(pmAtomValue *avp, int type, const char *buf)
{
    int	sts = __pmStringValue(buf, avp, type);

    if (sts == PM_ERR_TYPE) {
	fprintf(stderr, "The value \"%s\" is incompatible with the data "
			"type (PM_TYPE_%s)\n",
			buf, pmTypeStr(type));
	exit(1);
    }
    if (sts == -ERANGE) {
	fprintf(stderr, "The value \"%s\" is out of range for the data "
			"type (PM_TYPE_%s)\n",
			buf, pmTypeStr(type));
	exit(1);
    }
    if (sts < 0) {
	fprintf(stderr, "%s: cannot convert string value \"%s\": %s\n",
		    pmProgname, buf, pmErrStr(sts));
	exit(1);
    }
}

int
main(int argc, char **argv)
{
    int		sts;
    int		n;
    int		c;
    int		i;
    char	*p;
    char	*source;
    char	*namelist[1];
    pmID	pmidlist[1];
    pmResult	*result;
    char	**instnames = NULL;
    int		numinst = 0;
    int		force = 0;
    pmDesc	desc;
    pmAtomValue	nav;
    pmValueSet	*vsp;
    char        *subopt;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
        case 'f':
            force++;
            break;

	case 'i':	/* list of instances */
#define WHITESPACE ", \t\n"
	    subopt = strtok(opts.optarg, WHITESPACE);
	    while (subopt != NULL) {
		numinst++;
		n = numinst * sizeof(char *);
		instnames = (char **)realloc(instnames, n);
		if (instnames == NULL)
		    __pmNoMem("pmstore.instnames", n, PM_FATAL_ERR);
		instnames[numinst-1] = subopt;
		subopt = strtok(NULL, WHITESPACE);
	    }
#undef WHITESPACE
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors || opts.optind != argc - 2) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if (opts.context == PM_CONTEXT_HOST)
	source = opts.hosts[0];
    else if (opts.context == PM_CONTEXT_LOCAL)
	source = NULL;
    else {
	opts.context = PM_CONTEXT_HOST;
	source = "local:";
    }
    if ((sts = pmNewContext(opts.context, source)) < 0) {
	if (opts.context == PM_CONTEXT_LOCAL)
	    fprintf(stderr, "%s: Cannot make standalone local connection: %s\n",
		    pmProgname, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmProgname, source, pmErrStr(sts));
	exit(1);
    }

    namelist[0] = argv[opts.optind++];
    if ((n = pmLookupName(1, namelist, pmidlist)) < 0) {
	printf("%s: pmLookupName: %s\n", namelist[0], pmErrStr(n));
	exit(1);
    }
    if (pmidlist[0] == PM_ID_NULL) {
	printf("%s: unknown metric\n", namelist[0]);
	exit(1);
    }
    if ((n = pmLookupDesc(pmidlist[0], &desc)) < 0) {
	printf("%s: pmLookupDesc: %s\n", namelist[0], pmErrStr(n));
	exit(1);
    }
    if (desc.type == PM_TYPE_AGGREGATE || desc.type == PM_TYPE_AGGREGATE_STATIC) {
	fprintf(stderr, "%s: Cannot modify values for PM_TYPE_AGGREGATE metrics\n",
	    pmProgname);
	exit(1);
    }
    if (desc.type == PM_TYPE_EVENT || desc.type == PM_TYPE_HIGHRES_EVENT) {
	fprintf(stderr, "%s: Cannot modify values for event type metrics\n",
	    pmProgname);
	exit(1);
    }
    if (instnames != NULL) {
	pmDelProfile(desc.indom, 0, NULL);
	for (i = 0; i < numinst; i++) {
	    if ((n = pmLookupInDom(desc.indom, instnames[i])) < 0) {
		printf("pmLookupInDom %s[%s]: %s\n",
		    namelist[0], instnames[i], pmErrStr(n));
		exit(1);
	    }
	    if ((sts = pmAddProfile(desc.indom, 1, &n)) < 0) {
		printf("pmAddProfile %s[%s]: %s\n",
		    namelist[0], instnames[i], pmErrStr(sts));
		exit(1);
	    }
	}
    }
    if ((n = pmFetch(1, pmidlist, &result)) < 0) {
	printf("%s: pmFetch: %s\n", namelist[0], pmErrStr(n));
	exit(1);
    }

    /* value is argv[opts.optind] */
    mkAtom(&nav, desc.type, argv[opts.optind]);

    vsp = result->vset[0];
    if (vsp->numval < 0) {
	printf("%s: Error: %s\n", namelist[0], pmErrStr(vsp->numval));
	exit(1);
    }

    if (vsp->numval == 0) {
        if (!force) {
            printf("%s: No value(s) available!\n", namelist[0]);
            exit(1);
        }
        else {
            pmAtomValue tmpav;

            mkAtom(&tmpav, PM_TYPE_STRING, "(none)");

            vsp->numval = 1;
            vsp->valfmt = __pmStuffValue(&tmpav, &vsp->vlist[0], PM_TYPE_STRING);
        }
    }

    for (i = 0; i < vsp->numval; i++) {
	pmValue	*vp = &vsp->vlist[i];
	printf("%s", namelist[0]);
	if (desc.indom != PM_INDOM_NULL) {
	    if ((n = pmNameInDom(desc.indom, vp->inst, &p)) < 0)
		printf(" inst [%d]", vp->inst);
	    else {
		printf(" inst [%d or \"%s\"]", vp->inst, p);
		free(p);
	    }
	}
	printf(" old value=");
	pmPrintValue(stdout, vsp->valfmt, desc.type, vp, 1);
	vsp->valfmt = __pmStuffValue(&nav, &vsp->vlist[i], desc.type);
	printf(" new value=");
	pmPrintValue(stdout, vsp->valfmt, desc.type, vp, 1);
	putchar('\n');
    }
    if ((n = pmStore(result)) < 0) {
	printf("%s: pmStore: %s\n", namelist[0], pmErrStr(n));
	exit(1);
    }
    pmFreeResult(result);
    exit(0);
}
