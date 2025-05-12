/*
 * Copyright (c) 2016 Red Hat.
 * Copyright (c) 2022 Ken McDonell.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * Small derived metric exerciser.
 */
#include <pcp/pmapi.h>
#include "libpcp.h"

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("derived options"),
    { "", 0, 'f', NULL, "fetch values => -m as well" },
    { "", 0, 'm', NULL, "fetch metadata" },
    PMAPI_OPTIONS_END
};
static pmOptions opts = {
    .short_options = "D:mf?",
    .long_options = longopts,
    .short_usage = "[options] name=expr ...",
};

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    pmID	pmid;
    pmDesc	desc;
    char	*name, *expr;
    char	*errmsg;
    int		mflag = 0;
    int		fflag = 0;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'm':	/* try metadata */
	    mflag++;
	    break;	

	case 'f':	/* try fetch */
	    fflag++;
	    mflag++;
	    break;	
	}
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	return 1;
    }

    if (mflag || fflag) {
	if ((sts = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	    fprintf(stderr, "pmNewContext failed: %s\n", pmErrStr(sts));
	    exit(1);
	}
    }

    while (opts.optind < argc) {
	/* non-flag args are argv[optind] ... argv[argc-1] */
	name = expr = argv[opts.optind];
	if ((name = strsep(&expr, "=")) == NULL) {
	    fprintf(stderr, "%s: invalid name=expr \"%s\"\n", pmGetProgname(), expr);
	    return 1;
	}

	if (pmRegisterDerivedMetric(name, expr, &errmsg) < 0) {
	    fprintf(stderr, "%s: %s", pmGetProgname(), errmsg);
	    free(errmsg);
	}
	else {
	    printf("%s: registered \"%s\" as: \"%s\"\n", pmGetProgname(), name, expr);
	}

	if (mflag || fflag) {
	    if ((sts = pmLookupName(1, (const char **)&name, &pmid)) < 0) {
		fprintf(stderr, "pmLookupName failed: %s\n", pmErrStr(sts));
	    }
	    else {
		printf("    pmID: %s\n", pmIDStr(pmid));
		if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
		    fprintf(stderr, "pmLookupDesc failed: %s\n", pmErrStr(sts));
		    /*
		     * wish there was more we could do here, but libpcp offers
		     * no help ATM
		     */
		    ;
		}
		else {
		    pmPrintDesc(stdout, &desc);
		    if (fflag) {
			pmResult	*rp;
			if ((sts = pmFetch(1, &pmid, &rp)) < 0) {
			    fprintf(stderr, "pmFetch failed: %s\n", pmErrStr(sts));
			}
			else {
			    __pmDumpResult(stdout, rp);
			    pmFreeResult(rp);
			}
		    }
		}
	    }
	}

	opts.optind++;
    }

    return 0;
}
