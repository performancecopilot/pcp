/*
 * Exercise derived metric control options features.
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2026 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>

static pmLongOptions longopts[] = {
    { "add", 1, 'a', "spec", "pmAddDerivedMetric this one" },
    { "ctx", 0, 'c', NULL, "do pmNewContext" },
    PMOPT_DEBUG,	/* -D */
    { "env", 1, 'e', "control", "put PCP_DERIVED_CONTROL=control in the environment" },
    { "load", 1, 'l', "configfile", "pmLoadDerivedConfig this one" },
    { "register", 1, 'r', "spec", "pmRegisterDerivedMetric this one" },
    { "set", 1, 's', "control", "pmSetDerivedControl this one" },
    PMAPI_OPTIONS_END
};

static int overrides(int, pmOptions *);
static pmOptions opts = {
    .short_options = "a:cD:e:l:r:s:?",
    .long_options = longopts,
    .short_usage = "[options] metric-to-fetch ...",
    .override = overrides,
};

static int
overrides(int opt, pmOptions *optsp)
{
    /* ones I can handle ... */
    switch (opt) {
	case 'a':
	case 'c':
	case 'e':
	case 'l':
	case 'r':
	case 's':
		    return 1;

	default:
		    return 0;
    }
    /* NOTREACHED */
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		what;
    int		value;
    char	*p;
    char	*name;
    char	*expr;
    char	*errmsg;
    pmID	pmid;
    pmDesc	desc;
    pmResult	*rp;

    /*
     * take control of env ... no PCP_DERIVED_CONFIG for us unless we
     * do it with -e in a controlled manner
     */
    setenv("PCP_DERIVED_CONFIG", "", 1);

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'a':	/* pmAddDerivedMetric */
	    name = opts.optarg;
	    expr = strchr(name, '=');
	    if (expr == NULL) {
		printf("Error: -a missing = for expr (%s)\n", name);
		break;
	    }
	    *expr++ = '\0';
	    sts = pmAddDerivedMetric(name, expr, &errmsg);
	    if (sts == 0)
		printf("Added %s=%s\n", name, expr);
	    else
		printf("pmAddDerivedMetric(%s, %s, ...) failed: %s\n", name, expr, pmErrStr(sts));
	    break;	

	case 'e':	/* set in environment */    
	    name = opts.optarg;
	    expr = strchr(name, '=');
	    if (expr == NULL) {
		printf("Error: -e missing = for expr (%s)\n", name);
		break;
	    }
	    *expr++ = '\0';
	    if (setenv(name, expr, 1) == 0)
		printf("Set %s=%s\n", name, expr);
	    else
		printf("setenv(%s, %s, ...) failed: %s\n", name, expr, pmErrStr(-errno));
	    break;

	case 'c':	/* pmNewContext */
	    sts = pmNewContext(PM_CONTEXT_HOST, "localhost");
	    if (sts >= 0)
		printf("Context %d created\n", sts);
	    else
		printf("pmNewContext failed: %s\n", pmErrStr(sts));
	    break;	

	case 'l':	/* pmLoadDerivedConfig */
	    sts = pmLoadDerivedConfig(opts.optarg);
	    if (sts >= 0)
		printf("Loaded %s\n", opts.optarg);
	    else
		printf("pmLoadDerivedConfig failed: %s\n", pmErrStr(sts));
	    break;

	case 'r':	/* pmRegisterDerivedMetric */
	    name = opts.optarg;
	    expr = strchr(name, '=');
	    if (expr == NULL) {
		printf("Error: -r missing = for expr (%s)\n", name);
		break;
	    }
	    *expr++ = '\0';
	    sts = pmRegisterDerivedMetric(name, expr, &errmsg);
	    if (sts == 0)
		printf("Registered %s=%s\n", name, expr);
	    else
		printf("pmRegisterDerivedMetric(%s, %s, ...) failed: %s\n", name, expr, pmErrStr(sts));
	    break;	

	case 's':	/* pmSetDerivedControl */
	    name = opts.optarg;
	    expr = strchr(name, '=');
	    if (expr == NULL) {
		printf("Error: -s missing = for expr (%s)\n", name);
		break;
	    }
	    *expr++ = '\0';
	    value = (int)strtol(expr, &p, 10);
	    if (p == expr || *p != '\0') {
		printf("Error: -s expr (%s) not numeric\n", expr);
	    }
	    if (strcasecmp(name, "global_limit") == 0)
		what = PCP_DERIVED_GLOBAL_LIMIT;
	    else if (strcasecmp(name, "context_limit") == 0)
		what = PCP_DERIVED_CONTEXT_LIMIT;
	    else if (strcasecmp(name, "debug_syntax") == 0)
		what = PCP_DERIVED_DEBUG_SYNTAX;
	    else if (strcasecmp(name, "debug_semantics") == 0)
		what = PCP_DERIVED_DEBUG_SEMANTICS;
	    else if (strcasecmp(name, "debug_eval") == 0)
		what = PCP_DERIVED_DEBUG_EVAL;
	    else if (strcasecmp(name, "option_novalue") == 0)
		what = PCP_DERIVED_OPTION_NOVALUE;
	    else {
		printf("Error: -s cannot map %s to a PCP_DERIVED_* macro\n", name);
		break;
	    }
	    sts = pmSetDerivedControl(what, value);
	    if (sts == 0)
		printf("Set %s(%d)=%d\n", name, what, value);
	    else
		printf("pmSetDerivedControl(%d, %d) failed: %s\n", what, value, pmErrStr(sts));
	    break;	

	}
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    /* metrics are argv[opts.optind] ... argv[argc-1] */
    for ( ; opts.optind < argc; opts.optind++)  {
	name = argv[opts.optind];
	printf("%s:\n", name);
	if ((sts = pmLookupName(1, (const char **)&name, &pmid)) < 0) {
	    printf("pmLookupName failed: %s\n", pmErrStr(sts));
	    continue;
	}
	if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
	    printf("pmLookupDesc failed: %s\n", pmErrStr(sts));
	    continue;
	}
	pmPrintDesc(stdout, &desc);
	if ((sts = pmFetch(1, &pmid, &rp)) < 0) {
	    printf("pmFetch failed: %s\n", pmErrStr(sts));
	    continue;
	}
	printf("Fetched %d values\n", rp->vset[0]->numval);
    }

    return 0;
}
