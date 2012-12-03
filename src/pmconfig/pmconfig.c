/*
 * Copyright (c) 2012 Red Hat.
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

int
main(int argc, char **argv)
{
    int		c;
    int		errflag = 0;
    int		sflag = 0;
    int		Lflag = 0;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "alLs")) != EOF) {
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
	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] [variable ...]\n\
\n\
Options:\n\
  -a | -l      show all, unmodified format (default)\n\
  -L           show library features instead of environment\n\
  -s           show all, quoted format for shell expansion\n",
		pmProgname);
	exit(1);
    }

    if (optind >= argc) {
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
	    for (c = optind; c < argc; c++)
		printf("export %s=${%s:-\"%s\"}\n", argv[c], argv[c],
			__pmGetAPIConfig(argv[c]));
	else
	    for (c = optind; c < argc; c++)
		printf("export %s=${%s:-\"%s\"}\n", argv[c], argv[c],
			pmGetConfig(argv[c]));
    }
    else if (Lflag)
	for (c = optind; c < argc; c++)
	    printf("%s=%s\n", argv[c], __pmGetAPIConfig(argv[c]));
    else
	for (c = optind; c < argc; c++)
	    printf("%s=%s\n", argv[c], pmGetConfig(argv[c]));
   
    exit(0);
}
