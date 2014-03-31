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
#include "impl.h"

void
env_formatter(char *var, char *prefix, char *val)
{
    char *v;

    __pmNativeConfig(var, prefix, val);
    v = getenv(var);
    if (!v || v[0] == '\0')
	printf("%s=\n", var);
    else
	printf("%s=%s\n", var, v);
}

void
api_formatter(const char *var, const char *val)
{
    if (!val || val[0] == '\0')
	printf("%s=false\n", var);
    else
	printf("%s=%s\n", var, val);
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
    int		Lflag = 0;

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
	    Lflag = 1;
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

    if (opts.optind >= argc) {
	if (sflag)
	    putenv("SHELL=/bin/sh");
	if (Lflag)
	    __pmAPIConfig(api_formatter);
	else
	    __pmConfig(env_formatter);
    }
    else if (sflag) {
	putenv("SHELL=/bin/sh");
	if (Lflag)
	    for (c = opts.optind; c < argc; c++)
		printf("export %s=${%s:-\"%s\"}\n", argv[c], argv[c],
			__pmGetAPIConfig(argv[c]));
	else
	    for (c = opts.optind; c < argc; c++)
		printf("export %s=${%s:-\"%s\"}\n", argv[c], argv[c],
			pmGetConfig(argv[c]));
    }
    else if (Lflag)
	for (c = opts.optind; c < argc; c++)
	    printf("%s=%s\n", argv[c], __pmGetAPIConfig(argv[c]));
    else
	for (c = opts.optind; c < argc; c++)
	    printf("%s=%s\n", argv[c], pmGetConfig(argv[c]));
   
    exit(0);
}
