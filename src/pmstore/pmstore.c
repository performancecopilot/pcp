/*
 * pmstore [-Ff] [-h hostname ] [-i inst[,inst...]] [-n pmnsfile ] metric value
 *
 * Copyright (c) 2013-2016 Red Hat.
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
#include "libpcp.h"

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_DEBUG,
    PMOPT_HOST,
    PMOPT_LOCALPMDA,
    PMOPT_SPECLOCAL,
    PMOPT_NAMESPACE,
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Value options"),
    { "fetch", 0, 'F', 0, "perform pmFetch after pmStore to confirm value" },
    { "force", 0, 'f', 0, "store the value even if there is no current value set" },
    { "insts", 1, 'i', "INSTS", "restrict store to comma-separated list of instances" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_POSIX,
    .short_options = "D:Ffh:K:Li:n:V?",
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
		    pmGetProgname(), buf, pmErrStr(sts));
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
    pmResult	*old;
    pmResult	*new;
    pmResult	*store;
    char	**instnames = NULL;
    int		numinst = 0;
    int		force = 0;
    int		fetch = 0;
    int		old_force = 0;
    pmDesc	desc;
    pmAtomValue	nav;
    pmValueSet	*old_vsp;
    pmValueSet	*store_vsp;
    pmValueSet	*new_vsp;
    char        *subopt;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
        case 'f':
            force++;
            break;

        case 'F':
            fetch++;
            break;

	case 'i':	/* list of instances */
#define WHITESPACE ", \t\n"
	    subopt = strtok(opts.optarg, WHITESPACE);
	    while (subopt != NULL) {
		numinst++;
		n = numinst * sizeof(char *);
		instnames = (char **)realloc(instnames, n);
		if (instnames == NULL)
		    pmNoMem("pmstore.instnames", n, PM_FATAL_ERR);
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

    if (opts.errors ||
	opts.optind != argc - 2 ||
	(opts.flags & PM_OPTFLAG_EXIT)) {
	sts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(sts);
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
		    pmGetProgname(), pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmGetProgname(), source, pmErrStr(sts));
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
	    pmGetProgname());
	exit(1);
    }
    if (desc.type == PM_TYPE_EVENT || desc.type == PM_TYPE_HIGHRES_EVENT) {
	fprintf(stderr, "%s: Cannot modify values for event type metrics\n",
	    pmGetProgname());
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

    if ((n = pmFetch(1, pmidlist, &old)) < 0) {
	printf("%s: pre pmFetch: %s\n", namelist[0], pmErrStr(n));
	exit(1);
    }
    old_vsp = old->vset[0];
    if (old_vsp->numval < 0) {
	printf("%s: Error: %s\n", namelist[0], pmErrStr(old_vsp->numval));
	exit(1);
    }

    /* value is argv[opts.optind] */
    mkAtom(&nav, desc.type, argv[opts.optind]);

    /* duplicate old -> store */
    store = (pmResult *)malloc(sizeof(pmResult));
    if (store == NULL) {
	fprintf(stderr, "%s: pmResult malloc(%d) failed\n", pmGetProgname(), (int)sizeof(pmResult));
	exit(1);
    }
    store->timestamp = old->timestamp;
    store->numpmid = old->numpmid;
    store->vset[0] = (pmValueSet *)malloc(sizeof(pmValueSet)+(old_vsp->numval-1)*sizeof(pmValue));
    if (store->vset[0] == NULL) {
	fprintf(stderr, "%s: pmValueSet malloc(%d) failed\n", pmGetProgname(), (int)(sizeof(pmValueSet)+(old_vsp->numval-1)*sizeof(pmValue)));
	exit(1);
    }
    store_vsp = store->vset[0];
    store_vsp->pmid = old_vsp->pmid;
    store_vsp->numval = old_vsp->numval;
    store_vsp->valfmt = old_vsp->valfmt;
    for (i = 0; i < old_vsp->numval; i++) {
	/*
	 * OK to copy these, the __pmStuffValue() below will assign
	 * new values to the old-> ones
	 */
	store_vsp->vlist[i] = old_vsp->vlist[i];
    }

    if (old_vsp->numval == 0) {
        if (!force) {
            printf("%s: No value(s) available!\n", namelist[0]);
            exit(1);
        }
        else {
	    /*
	     * No current values, so fake one, so that we allocate space
	     * and then can clobber it in the pmResult (store) below.
	     */
            pmAtomValue tmpav;

            mkAtom(&tmpav, PM_TYPE_STRING, "(none)");

            store_vsp->numval = 1;
            store_vsp->valfmt = __pmStuffValue(&tmpav, &store_vsp->vlist[0], PM_TYPE_STRING);
	    old_force = 1;
        }
    }

    for (i = 0; i < store_vsp->numval; i++) {
	store_vsp->valfmt = __pmStuffValue(&nav, &store_vsp->vlist[i], desc.type);
    }

    if ((n = pmStore(store)) < 0) {
	printf("%s: new value=\"%s\" pmStore: %s\n", namelist[0], argv[opts.optind], pmErrStr(n));
	exit(1);
    }

    if (fetch) {
	if ((n = pmFetch(1, pmidlist, &new)) < 0) {
	    printf("%s: post pmFetch: %s\n", namelist[0], pmErrStr(n));
	    exit(1);
	}
    }
    else
	new = store;

    new_vsp = new->vset[0];

    for (i = 0; i < new_vsp->numval; i++) {
	pmValue	*old_vp = &old_vsp->vlist[i];
	pmValue	*new_vp = &new_vsp->vlist[i];
	printf("%s", namelist[0]);
	if (desc.indom != PM_INDOM_NULL) {
	    if ((n = pmNameInDom(desc.indom, new_vp->inst, &p)) < 0)
		printf(" inst [%d]", new_vp->inst);
	    else {
		printf(" inst [%d or \"%s\"]", new_vp->inst, p);
		free(p);
	    }
	}
	printf(" old value=");
	if (old_force == 0)
	    pmPrintValue(stdout, old_vsp->valfmt, desc.type, old_vp, 1);
	else
	    printf("\"(none)\"");
	printf(" new value=");
	pmPrintValue(stdout, new_vsp->valfmt, desc.type, new_vp, 1);
	putchar('\n');
    }

    exit(0);
}
