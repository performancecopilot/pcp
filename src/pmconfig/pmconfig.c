/*
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
formatter(char *var, char *prefix, char *val)
{
    char *v;

    __pmNativeConfig(var, prefix, val);
    v = getenv(var);
    if (!v || v[0] == '\0')
	printf("%s=\n", var);
    else
	printf("%s=%s\n", var, v);
}

int
main(int argc, char **argv)
{
    int		c;
    int		errflag = 0;
    int		sflag = 0;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "als")) != EOF) {
        switch (c) {
        case 'a':       /* show all, defualt (unmodified) dump format */
	case 'l':
	    sflag = 0;
	    break;
        case 's':       /* show all, guarded format for shell expansion */
	    sflag = 1;
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
  -s           show all, quoted format for shell expansion\n",
		pmProgname);
	exit(1);
    }

    if (optind >= argc) {
	if (sflag)
	    putenv("SHELL=/bin/sh");
	__pmConfig(formatter);
    }
    else if (sflag) {
	putenv("SHELL=/bin/sh");
	for (c = optind; c < argc; c++)
	    printf("export %s=${%s:-\"%s\"}\n", argv[c], argv[c],
			pmGetConfig(argv[c]));
    }
    else {
	for (c = optind; c < argc; c++)
	    printf("%s=%s\n", argv[c], pmGetConfig(argv[c]));
    }
    exit(0);
}
