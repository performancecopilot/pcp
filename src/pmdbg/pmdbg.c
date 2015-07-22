/*
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 1995,2002-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

/*
 * pmdbg - help for PCP debug flags
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"

static struct {
    int		flag;
    char	*name;
    char	*text;
} foo[] = {
    { DBG_TRACE_PDU,	"PDU",
		"Trace PDU traffic at the Xmit and Recv level" },
    { DBG_TRACE_FETCH,	"FETCH",
		"Dump results from pmFetch" },
    { DBG_TRACE_PROFILE,	"PROFILE",
		"Trace changes and xmits for instance profile" },
    { DBG_TRACE_VALUE,		"VALUE",
		"Diags for metric value extraction and conversion" },
    { DBG_TRACE_CONTEXT,	"CONTEXT",
		"Trace changes to contexts" },
    { DBG_TRACE_INDOM,		"INDOM",
		"Low-level instance profile xfers" },
    { DBG_TRACE_PDUBUF,		"PDUBUF",
		"Trace pin/unpin ops for PDU buffers" },
    { DBG_TRACE_LOG,		"LOG",
		"Diags for archive log manipulations" },
    { DBG_TRACE_LOGMETA,	"LOGMETA",
		"Diags for meta-data operations on archive logs" },
    { DBG_TRACE_OPTFETCH,	"OPTFETCH",
		"Trace optFetch magic" },
    { DBG_TRACE_AF,		"AF",
		"Trace asynchronous event scheduling" },
    { DBG_TRACE_APPL0,		"APPL0",
		"Application-specific flag 0" },
    { DBG_TRACE_APPL1,		"APPL1",
		"Application-specific flag 1" },
    { DBG_TRACE_APPL2,		"APPL2",
		"Application-specific flag 2" },
    { DBG_TRACE_PMNS,		"PMNS",
		"Diags for PMNS manipulations" },
    { DBG_TRACE_LIBPMDA,        "LIBPMDA",
	        "Trace PMDA callbacks in libpcp_pmda" },
    { DBG_TRACE_TIMECONTROL,	"TIMECONTROL",
    		"Trace Time Control API" },
    { DBG_TRACE_PMC,		"PMC",
    		"Trace metrics class operations" },
    { DBG_TRACE_DERIVE,		"DERIVE",
    		"Derived metrics operations" },
    { DBG_TRACE_LOCK,		"LOCK",
    		"Trace locks (if multi-threading enabled)" },
    { DBG_TRACE_INTERP,		"INTERP",
		"Diags for value interpolation in archives" },
    { DBG_TRACE_CONFIG,		"CONFIG",
		"Trace config initialization from pmGetConfig" },
    { DBG_TRACE_LOOP,		"LOOP",
		"Diags for pmLoop* services" },
    { DBG_TRACE_FAULT,		"FAULT",
    		"Trace fault injection (if enabled)" },
    { DBG_TRACE_AUTH,		"AUTH",
    		"Authentication services (if enabled)" },
    { DBG_TRACE_DISCOVERY,	"DISCOVERY",
    		"Service discovery (if enabled)" },
    { DBG_TRACE_ATTR,		"ATTR",
    		"Trace connection attributes" },
    { DBG_TRACE_DESPERATE,		"DESPERATE",
    		"Desperate/verbose level" },
};

static int	nfoo = sizeof(foo) / sizeof(foo[0]);

static char	*fmt = "DBG_TRACE_%-11.11s %7d  %s\n";

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_DEBUG,
    { "list", 0, 'l', 0, "displays mnemonic and decimal values of PCP debug bitfields" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:l?",
    .long_options = longopts,
    .short_usage = "[options] [code ..]",
};

int
main(int argc, char **argv)
{
    int		i;
    int		c;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'l':	/* list all flags */
	    printf("Performance Co-Pilot Debug Flags\n");
	    printf("#define                 Value  Meaning\n");
	    for (i = 0; i < nfoo; i++)
		printf(fmt, foo[i].name, foo[i].flag, foo[i].text);
	    exit(0);
	    /*NOTREACHED*/

	case 'D':
	    if ((i = __pmParseDebug(opts.optarg)) < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n", pmProgname, opts.optarg);
		exit(1);
	    }
	    else 
		printf("%s = %d\n", opts.optarg, i);
	    exit(0);
	    /*NOTREACHED*/

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors || opts.optind >= argc) {
	pmUsageMessage(&opts);
	exit(1);
    }

    /* non-flag args are argv[opts.optind] ... argv[argc-1] */
    while (opts.optind < argc) {
	char	*p = argv[opts.optind];
	for (p = argv[opts.optind]; *p && isdigit((int)*p); p++)
	    ;
	if (*p == '\0')
	    sscanf(argv[opts.optind], "%d", &c);
	else {
	    char	*q;
	    p = argv[opts.optind];
	    if (*p == '0' && (p[1] == 'x' || p[1] == 'X'))
		p = &p[2];
	    for (q = p; isxdigit((int)*q); q++)
		;
	    if (*q != '\0' || sscanf(p, "%x", &c) != 1) {
		printf("Cannot decode \"%s\" - neither decimal nor hexadecimal\n", argv[opts.optind]);
		goto next;
	    }
	}
	printf("Performance Co-Pilot -- pmDebug value = %d (0x%x)\n", c, c);
	printf("#define                 Value  Meaning\n");
	for (i = 0; i < nfoo; i++) {
	    if (c & foo[i].flag)
		printf(fmt, foo[i].name, foo[i].flag, foo[i].text);
	}

next:
	opts.optind++;
    }

    return 0;
}
