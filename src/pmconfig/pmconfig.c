/*
 * Copyright (c) 2012,2014 Red Hat.
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
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

static int apiflag;
static const char *empty = "";
static const char *none = "false";

static void
direct_report(const char *var, const char *val)
{
    if (!val || val[0] == '\0')
	val = empty;
    printf("%s=%s\n", var, val);
}

static void
export_report(const char *var, const char *val)
{
    char buffer[4096];
    const char *p;
    int i = 0;

    if (!val || val[0] == '\0')
        val = empty;
    else {
	/* ensure we do not leak any problematic characters into export */
	for (p = val; *p != '\0' && i < sizeof(buffer)-6; p++) {
	    if ((int)*p == '\'') {
		buffer[i++] = '\'';
		buffer[i++] = '\\';
		buffer[i++] = '\'';
		buffer[i++] = '\'';
	    } else {
		buffer[i++] = *p;
	    }
	}
	buffer[i] = '\0';
	val = buffer;
    }
    if (apiflag)	/* API mode: no override allowed */
	printf("export %s='%s'\n", var, val);
    else
	printf("export %s=${%s:-'%s'}\n", var, var, val);
}

static void
pcp_conf_extract(char *var, char *prefix, char *val)
{
    __pmNativeConfig(var, prefix, val);
    val = getenv(var);
    direct_report(var, val);
}

static void
pcp_conf_shell_extract(char *var, char *prefix, char *val)
{
    __pmNativeConfig(var, prefix, val);
    val = getenv(var);
    export_report(var, val);
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "all", 0, 'a', 0, "show all, unmodified format (default)" },
    { "list", 0, 'l', 0, "synonym for showing \"all\" (above)" },
    { "library", 0, 'L', 0, "show library features instead of environment" },
    { "shell", 0, 's', 0, "show all, quoted format for shell expansion" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "alLs?",
    .long_options = longopts,
    .short_usage = "[variable ...]",
};

int
main(int argc, char **argv)
{
    int		c;
    int		sflag = 0;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
        switch (c) {
	case 'a':       /* show all, default (unmodified) list format */
	case 'l':
	    sflag = 0;
	    break;
	case 's':       /* show all, guarded format for shell expansion */
	    sflag = 1;
	    break;
	case 'L':
	    apiflag = 1;
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

    if (sflag) {
	putenv("SHELL=/bin/sh");
	empty = none;
    }

    /* the complete list of variables is to be reported */
    if (opts.optind >= argc) {
	if (apiflag)
	    __pmAPIConfig(sflag ? export_report : direct_report);
	else
	    __pmConfig(sflag ? pcp_conf_shell_extract : pcp_conf_extract);
	exit(0);
    }

    /* an explicit list of variables has been requested */
    if (sflag) {
	if (apiflag)
	    for (c = opts.optind; c < argc; c++)
		export_report(argv[c], pmGetAPIConfig(argv[c]));
	else
	    for (c = opts.optind; c < argc; c++)
		export_report(argv[c], pmGetConfig(argv[c]));
    } else {
	if (apiflag)
	    for (c = opts.optind; c < argc; c++)
		direct_report(argv[c], pmGetAPIConfig(argv[c]));
	else
	    for (c = opts.optind; c < argc; c++)
		direct_report(argv[c], pmGetConfig(argv[c]));
    }
    exit(0);
}
