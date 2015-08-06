/*
 * Copyright (c) 2014-2015 Red Hat.
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
#include <ctype.h>

int
main(int argc, char **argv)
{
    int		code;
    int		sts;
    char	*p;
    char	*q;

    if (argc > 1 &&
	(strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--list") == 0)) {
	__pmDumpErrTab(stdout); 
	exit(1);
    }
    else if (argc > 1 &&
	(strcmp(argv[1], "-?") == 0 || strcmp(argv[1], "--help") == 0)) {
	argc = 0;
    }
    else if (argc > 1 && argv[1][0] == '-' && !isxdigit((int)argv[1][1])) { 
	fprintf(stderr, "Illegal option -- %s\n", &argv[1][1]);
	argc = 0;
    }

    if (argc == 0) {
	fprintf(stderr,
"Usage: pmerr [options] [code]\n\n"
"  -l, --list   causes all known error codes to be listed\n");
	exit(1);
    }

    while (argc > 1) {
	sts = 0;
	p = argv[1];
	if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
	    p = &p[2];
	    for (q = p; isxdigit((int)*q); q++)
		;
	    if (*q == '\0')
		sts = sscanf(p, "%x", &code);
	}
	if (sts < 1)
	    sts = sscanf(argv[1], "%d", &code);
	if (sts != 1) {
	    printf("Cannot decode \"%s\" - neither decimal nor hexadecimal\n", argv[1]);
	    goto next;
	}

	if (code > 0) {
	    code = -code;
	    printf("Code is positive, assume you mean %d\n", code);
	}

	printf("Code: %d 0x%x Text: %s\n", code, code, pmErrStr(code));

next:
	argc--;
	argv++;
    }

    return 0;
}
