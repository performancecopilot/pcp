/*
 * Copyright (c) 2013-2014,2016 Red Hat.
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
 * pmdbg - help for PCP debug fields
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "../libpcp/src/pmdbg.h"

static char	*fmt_old = "DBG_TRACE_%-11.11s  %10d  %s\n";
static char	*fmt = "%-14.14s  %s\n";

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_DEBUG,
    { "list", 0, 'l', 0, "display values and text for all PCP debug options" },
    { "old", 0, 'o', 0, "old and deprecated (bit-field) encodings" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:lo?",
    .long_options = longopts,
    .short_usage = "[options] [code ..]",
};

int
main(int argc, char **argv)
{
    int		i;
    int		c;
    int		old = 0;
    int		list = 0;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'l':	/* list all options */
	    list = 1;
	    break;

	case 'o':	/* old bit-field variants */
	    old = 1;
	    break;

	case 'D':
	    if ((i = __pmParseDebug(opts.optarg)) < 0) {
		fprintf(stderr, "%s: unrecognized debug option specification (%s)\n", pmProgname, opts.optarg);
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

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if (list) {
	printf("Performance Co-Pilot Debug Options\n");
	if (old)
	    printf("#define       Value  Meaning\n");
	else
	    printf(fmt, "Option", "Meaning");
	for (i = 0; i < num_debug; i++) {
	    if (!old) {
		printf(fmt, debug_map[i].name, debug_map[i].text);
	    }
	    else {
		if (debug_map[i].bit) {
		    char	*p;
		    char	*name = strdup(debug_map[i].name);
		    for (p = name; p != NULL && *p; p++)
			*p = toupper((int)*p);
		    printf(fmt_old, name, debug_map[i].bit, debug_map[i].text);
		    free(name);
		}
	    }
	}
	exit(0);
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
	for (i = 0; i < num_debug; i++) {
	    if (c & debug_map[i].bit) {
		char	*p;
		char	*name = strdup(debug_map[i].name);
		for (p = name; p != NULL && *p; p++)
		    *p = toupper((int)*p);
		printf(fmt_old, name, debug_map[i].bit, debug_map[i].text);
		free(name);
	    }
	}

next:
	opts.optind++;
    }

    return 0;
}
