/*
 * Copyright (c) 2013-2015 Red Hat.
 * Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <limits.h>

static void myeventdump(pmValueSet *, int, int);
static int  myoverrides(int, pmOptions *);

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_ARCHIVE,
    PMOPT_DEBUG,
    PMOPT_HOST,
    PMOPT_CONTAINER,
    PMOPT_LOCALPMDA,
    PMOPT_SPECLOCAL,
    PMOPT_NAMESPACE,
    PMOPT_UNIQNAMES,
    PMOPT_ORIGIN,
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Protocol options"),
    { "batch",    1, 'b', "N", "fetch N metrics at a time for -f and -v [20]" },
    { "desc",     0, 'd', 0, "get and print metric description" },
    { "fetch",    0, 'f', 0, "fetch and print values for all instances" },
    { "fetchall", 0, 'F', 0, "fetch and print values for non-enumerable indoms" },
    { "pmid",     0, 'm', 0, "print PMID" },
    { "fullpmid", 0, 'M', 0, "print PMID in verbose format" },
    { "oneline",  0, 't', 0, "get and display (terse) oneline text" },
    { "helptext", 0, 'T', 0, "get and display (verbose) help text" },
    PMAPI_OPTIONS_HEADER("Metrics options"),
    { "derived",  1, 'c', "FILE", "load derived metric definitions from FILE(s)" },
    { "events",   0, 'x', 0, "unpack and report on any fetched event records" },
    { "verify",   0, 'v', 0, "verify mode, be quiet and only report errors" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ,
    .short_options = "a:b:c:dD:Ffh:K:LMmN:n:O:tTvxzZ:?",
    .long_options = longopts,
    .short_usage = "[options] [metricname ...]",
    .override = myoverrides,
};

static int	p_mid;		/* Print metric IDs of leaf nodes */
static int	p_fullmid;	/* Print verbose metric IDs of leaf nodes */
static int	p_desc;		/* Print descriptions for metrics */
static int	p_oneline;	/* fetch oneline text? */
static int	p_help;		/* fetch help text? */
static int	p_value;	/* pmFetch and print value(s)? */
static int	p_force;	/* pmFetch and print value(s)? for non-enumerable indoms */

static int	need_context;	/* set if need a pmapi context */
static int	need_pmid;	/* set if need to lookup names */
static char	**namelist;
static pmID	*pmidlist;
static int	batchsize = 20;
static int	batchidx;
static int	verify;		/* Only print error messages */
static int	events;		/* Decode event metrics */
static pmID	pmid_flags;
static pmID	pmid_missed;

/*
 * pminfo has a few options which do not follow the defacto standards
 */
static int
myoverrides(int opt, pmOptions *opts)
{
    if (opt == 't' || opt == 'T')
	return 1;	/* we've claimed these, inform pmGetOptions */
    return 0;
}

/*
 * Cache all of the most recently requested pmInDom
 */
static char *
lookup(pmInDom indom, int inst)
{
    static pmInDom	last = PM_INDOM_NULL;
    static int		numinst = -1;
    static int		*instlist;
    static char		**namelist;
    int			i;

    if (indom != last) {
	if (numinst > 0) {
	    free(instlist);
	    free(namelist);
	}
	numinst = pmGetInDom(indom, &instlist, &namelist);
	last = indom;
    }

    for (i = 0; i < numinst; i++) {
	if (instlist[i] == inst)
	    return namelist[i];
    }

    return NULL;
}

/* 
 * we only ever have one metric
 */
static void
mydump(pmDesc *dp, pmValueSet *vsp, char *indent)
{
    int		j;
    char	*p;

    if (indent != NULL)
	printf("%s", indent);
    if (vsp->numval == 0) {
	printf("No value(s) available!\n");
	return;
    }
    else if (vsp->numval < 0) {
	printf("Error: %s\n", pmErrStr(vsp->numval));
	return;
    }

    for (j = 0; j < vsp->numval; j++) {
	pmValue	*vp = &vsp->vlist[j];
	if (dp->indom != PM_INDOM_NULL) {
	    if ((p = lookup(dp->indom, vp->inst)) == NULL) {
		if (p_force) {
		    /* the instance disappeared; ignore it */
		    printf("    inst [%d \"%s\"]\n", vp->inst, "DISAPPEARED");
		    continue;
		}
		else {
		    /* report the error and give up */
		    printf("pmNameIndom: indom=%s inst=%d: %s\n",
			    pmInDomStr(dp->indom), vp->inst, pmErrStr(PM_ERR_INST));
		    printf("    inst [%d]", vp->inst);
		}
	    }
	    else
		printf("    inst [%d or \"%s\"]", vp->inst, p);
	}
	else
	    printf("   ");
	printf(" value ");
	pmPrintValue(stdout, vsp->valfmt, dp->type, vp, 1);
	putchar('\n');
	if (!events)
	    continue;
	if (dp->type == PM_TYPE_HIGHRES_EVENT)
	    myeventdump(vsp, j, 1);
	else if (dp->type == PM_TYPE_EVENT)
	    myeventdump(vsp, j, 0);
    }
}

static void
setup_event_derived_metrics(void)
{
    if (pmid_flags == 0) {
	/*
	 * get PMID for event.flags and event.missed
	 * note that pmUnpackEventRecords() will have called
	 * __pmRegisterAnon(), so the anonymous metrics
	 * should now be in the PMNS
	 */
	char	*name_flags = "event.flags";
	char	*name_missed = "event.missed";
	int	sts;

	sts = pmLookupName(1, &name_flags, &pmid_flags);
	if (sts < 0) {
	    /* should not happen! */
	    fprintf(stderr, "Warning: cannot get PMID for %s: %s\n",
			name_flags, pmErrStr(sts));
	    /* avoid subsequent warnings ... */
	    __pmid_int(&pmid_flags)->item = 1;
	}
	sts = pmLookupName(1, &name_missed, &pmid_missed);
	if (sts < 0) {
	    /* should not happen! */
	    fprintf(stderr, "Warning: cannot get PMID for %s: %s\n",
			name_missed, pmErrStr(sts));
	    /* avoid subsequent warnings ... */
	    __pmid_int(&pmid_missed)->item = 1;
	}
    }
}

static int
dump_nparams(int numpmid)
{
    if (numpmid == 0) {
	printf(" ---\n");
	printf("	No parameters\n");
	return -1;
    }
    if (numpmid < 0) {
	printf(" ---\n");
	printf("	Error: illegal number of parameters (%d)\n", numpmid);
	return -1;
    }
    return 0;
}

static void
dump_parameter(pmValueSet *xvsp, int index, int *flagsp)
{
    int		sts, flags = *flagsp;
    pmDesc	desc;
    char	**names;

    if ((sts = pmNameAll(xvsp->pmid, &names)) >= 0) {
	if (index == 0) {
	    if (xvsp->pmid == pmid_flags) {
		flags = *flagsp = xvsp->vlist[0].value.lval;
		printf(" flags 0x%x", flags);
		printf(" (%s) ---\n", pmEventFlagsStr(flags));
		free(names);
		return;
	    }
	    else
		printf(" ---\n");
	}
	if ((flags & PM_EVENT_FLAG_MISSED) &&
	    (index == 1) &&
	    (xvsp->pmid == pmid_missed)) {
	    printf("        ==> %d missed event records\n",
			xvsp->vlist[0].value.lval);
	    free(names);
	    return;
	}
	printf("    ");
	__pmPrintMetricNames(stdout, sts, names, " or ");
	printf(" (%s)\n", pmIDStr(xvsp->pmid));
	free(names);
    }
    else
	printf("	PMID: %s\n", pmIDStr(xvsp->pmid));
    if ((sts = pmLookupDesc(xvsp->pmid, &desc)) < 0)
	printf("	pmLookupDesc: %s\n", pmErrStr(sts));
    else
	mydump(&desc, xvsp, "    ");
}

static void
myeventdump(pmValueSet *vsp, int inst, int highres)
{
    int		r;		/* event records */
    int		p;		/* event parameters */
    int		nrecords;
    int		flags;

    if (highres) {
	pmHighResResult	**hr;

	if ((nrecords = pmUnpackHighResEventRecords(vsp, inst, &hr)) < 0) {
	    fprintf(stderr, "pmUnpackHighResEventRecords: %s\n",
		    pmErrStr(nrecords));
	    return;
	}
	setup_event_derived_metrics();
	for (r = 0; r < nrecords; r++) {
	    printf("    --- event record [%d] timestamp ", r);
	    __pmPrintHighResStamp(stdout, &hr[r]->timestamp);
	    if (dump_nparams(hr[r]->numpmid) < 0)
		continue;
	    flags = 0;
	    for (p = 0; p < hr[r]->numpmid; p++)
		dump_parameter(hr[r]->vset[p], p, &flags);
	}
	pmFreeHighResEventResult(hr);
    }
    else {
	pmResult	**res;

	if ((nrecords = pmUnpackEventRecords(vsp, inst, &res)) < 0) {
	    fprintf(stderr, "pmUnpackEventRecords: %s\n", pmErrStr(nrecords));
	    return;
	}
	setup_event_derived_metrics();
	for (r = 0; r < nrecords; r++) {
	    printf("    --- event record [%d] timestamp ", r);
	    __pmPrintStamp(stdout, &res[r]->timestamp);
	    if (dump_nparams(res[r]->numpmid) < 0)
		continue;
	    flags = 0;
	    for (p = 0; p < res[r]->numpmid; p++)
		dump_parameter(res[r]->vset[p], p, &flags);
	}
	pmFreeEventResult(res);
    }
}

static void
report(void)
{
    int		i;
    int		sts;
    pmDesc	desc;
    pmResult	*result = NULL;
    pmResult	*xresult = NULL;
    pmValueSet	*vsp = NULL;
    char	*buffer;
    int		all_count;
    int		*all_inst;
    char	**all_names;

    if (batchidx == 0)
	return;

    /* Lookup names. 
     * Cull out names that were unsuccessfully looked up. 
     * However, it is unlikely to fail because names come from a traverse PMNS. 
     */
    if (need_pmid) {
        if ((sts = pmLookupName(batchidx, namelist, pmidlist)) < 0) {
	    int j = 0;
	    for (i = 0; i < batchidx; i++) {
		if (pmidlist[i] == PM_ID_NULL) {
		    printf("%s: pmLookupName: %s\n", namelist[i], pmErrStr(sts));
		    free(namelist[i]);
		}
		else {
		    /* assert(j <= i); */
		    pmidlist[j] = pmidlist[i];
		    namelist[j] = namelist[i];
		    j++;
		}
	    }
	    batchidx = j;
	}
    }

    if (p_value || verify) {
	if (opts.context == PM_CONTEXT_ARCHIVE) {
	    if ((sts = pmSetMode(PM_MODE_FORW, &opts.origin, 0)) < 0) {
		fprintf(stderr, "%s: pmSetMode failed: %s\n", pmProgname, pmErrStr(sts));
		exit(1);
	    }
	}
    	if ((sts = pmFetch(batchidx, pmidlist, &result)) < 0) {
	    for (i = 0; i < batchidx; i++)
		printf("%s: pmFetch: %s\n", namelist[i], pmErrStr(sts));
	    goto done;
	}
    }

    for (i = 0; i < batchidx; i++) {

	if (p_desc || p_value || verify) {
	    if ((sts = pmLookupDesc(pmidlist[i], &desc)) < 0) {
		printf("%s: pmLookupDesc: %s\n", namelist[i], pmErrStr(sts));
		continue;
	    }
	}

	if (p_desc || p_help || p_value)
	    /* Not doing verify, output separator  */
	    putchar('\n');


	if (p_value || verify) {
	    vsp = result->vset[i];
	    if (p_force) {
		if (result->vset[i]->numval == PM_ERR_PROFILE) {
		    /* indom is non-enumerable; try harder */
		    if ((all_count = pmGetInDom(desc.indom, &all_inst, &all_names)) > 0) {
			pmDelProfile(desc.indom, 0, NULL);
			pmAddProfile(desc.indom, all_count, all_inst);
			if (xresult != NULL) {
			    pmFreeResult(xresult);
			    xresult = NULL;
			}
			if (opts.context == PM_CONTEXT_ARCHIVE) {
			    if ((sts = pmSetMode(PM_MODE_FORW, &opts.origin, 0)) < 0) {
				fprintf(stderr, "%s: pmSetMode failed: %s\n", pmProgname, pmErrStr(sts));
				exit(1);
			    }
			}
			if ((sts = pmFetch(1, &pmidlist[i], &xresult)) < 0) {
			    printf("%s: pmFetch: %s\n", namelist[i], pmErrStr(sts));
			    continue;
			}
			vsp = xresult->vset[0];
			/* leave the profile in the default state */
			free(all_inst);
			free(all_names);
			pmDelProfile(desc.indom, 0, NULL);
			pmAddProfile(desc.indom, 0, NULL);
		    }
		    else if (all_count == 0) {
			printf("%s: pmGetIndom: No instances?\n", namelist[i]);
			continue;
		    }
		    else {
			printf("%s: pmGetIndom: %s\n", namelist[i], pmErrStr(all_count));
			continue;
		    }
		}
	    }
	}

	if (verify) {
	    if (desc.type == PM_TYPE_NOSUPPORT)
		printf("%s: Not Supported\n", namelist[i]);
	    else if (vsp->numval < 0)
		printf("%s: %s\n", namelist[i], pmErrStr(vsp->numval));
	    else if (vsp->numval == 0)
		printf("%s: No value(s) available\n", namelist[i]);
	    continue;
	}
	else
	    /* not verify */
	    printf("%s", namelist[i]);

	if (p_mid)
	    printf(" PMID: %s", pmIDStr(pmidlist[i]));
	if (p_fullmid)
	    printf(" = %d = 0x%x", pmidlist[i], pmidlist[i]);

	if (p_oneline) {
	    if ((sts = pmLookupText(pmidlist[i], PM_TEXT_ONELINE, &buffer)) == 0) {
		if (p_fullmid)
		    printf("\n    ");
		else
		    putchar(' ');
		printf("[%s]", buffer);
		free(buffer);
	    }
	    else
		printf(" One-line Help: Error: %s\n", pmErrStr(sts));
	}
	putchar('\n');

	if (p_desc)
	    __pmPrintDesc(stdout, &desc);

	if (p_help) {
	    if ((sts = pmLookupText(pmidlist[i], PM_TEXT_HELP, &buffer)) == 0) {
		char	*p;
		for (p = buffer; *p; p++)
		    ;
		while (p > buffer && p[-1] == '\n') {
		    p--;
		    *p = '\0';
		}
		if (*buffer != '\0') {
		    printf("Help:\n");
		    printf("%s", buffer);
		    putchar('\n');
		}
		else
		    printf("Help: <empty entry>\n");
		free(buffer);
	    }
	    else
		printf("Full Help: Error: %s\n", pmErrStr(sts));
	}

	if (p_value) {
	    mydump(&desc, vsp, NULL);
	}
    }

    if (result != NULL) {
	pmFreeResult(result);
	result = NULL;
    }
    if (xresult != NULL) {
	pmFreeResult(xresult);
	xresult = NULL;
    }

done:
    for (i = 0; i < batchidx; i++)
	free(namelist[i]);
    batchidx = 0;
}

static void
dometric(const char *name)
{
    if (*name == '\0') {
	printf("PMNS appears to be empty!\n");
	return;
    }

    namelist[batchidx]= strdup(name);
    if (namelist[batchidx] == NULL) {
	fprintf(stderr, "%s: namelist string malloc: %s\n", pmProgname, osstrerror());
	exit(1);
    }

    batchidx++;
    if (batchidx >= batchsize)
	report();
}

int
main(int argc, char **argv)
{
    int		a, c;
    int		sts;
    int		exitsts = 0;
    char	*source;
    char	*endnum;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	    case 'b':		/* batchsize */
		batchsize = (int)strtol(opts.optarg, &endnum, 10);
		if (*endnum != '\0') {
		    pmprintf("%s: -b requires numeric argument\n", pmProgname);
		    opts.errors++;
		}
		break;

	    case 'c':		/* derived metrics config file */
		sts = pmLoadDerivedConfig(opts.optarg);
		if (sts < 0) {
		    fprintf(stderr, "%s: derived configuration(s) error: %s\n", pmProgname, pmErrStr(sts));
		    /* errors are not necessarily fatal ... */
		}
		break;

	    case 'd':
		p_desc = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'F':
		p_force = p_value = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'f':
		p_value = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'M':
		p_fullmid = 1;
		p_mid = 1;
		need_pmid = 1;
		break;

	    case 'm':
		p_mid = 1;
		need_pmid = 1;
		break;

	    case 't':
		p_oneline = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'T':
		p_help = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'v':
		verify = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'x':
		events = p_value = 1;
		need_context = 1;
		need_pmid = 1;
		break;
	}
    }
    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if (opts.context)
	need_context = 1;

    if (opts.context == PM_CONTEXT_ARCHIVE)
	/*
	 * for archives, one metric per batch and start at beginning of
	 * archive for each batch so metric will be found if it is in
	 * the archive
	 */
	batchsize = 1;

    if (verify)
	p_desc = p_mid = p_fullmid = p_help = p_oneline = p_value = p_force = 0;



    if ((namelist = (char **)malloc(batchsize * sizeof(char *))) == NULL) {
	fprintf(stderr, "%s: namelist malloc: %s\n", pmProgname, osstrerror());
	exit(1);
    }

    if ((pmidlist = (pmID *)malloc(batchsize * sizeof(pmID))) == NULL) {
	fprintf(stderr, "%s: pmidlist malloc: %s\n", pmProgname, osstrerror());
	exit(1);
    }

    if (!opts.nsflag)
	need_context = 1; /* for distributed PMNS as no PMNS file given */

    if (need_context) {
	int ctxid;

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
	if ((sts = pmNewContext(opts.context, source)) < 0) {
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
	ctxid = sts;

	if (opts.context == PM_CONTEXT_ARCHIVE) {
	    pmTrimNameSpace();
	    /* complete TZ and time window option (origin) setup */
	    if (pmGetContextOptions(ctxid, &opts)) {
		pmflush();
		exit(1);
	    }
	}
    }

    if (opts.optind >= argc) {
    	sts = pmTraversePMNS("", dometric);
	if (sts < 0) {
	    fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		exitsts = 1;
	}
    }
    else {
	for (a = opts.optind; a < argc; a++) {
	    sts = pmTraversePMNS(argv[a], dometric);
	    if (sts < 0) {
		fprintf(stderr, "Error: %s: %s\n", argv[a], pmErrStr(sts));
		exitsts = 1;
	    }
	}
    }
    report();

    exit(exitsts);
}
