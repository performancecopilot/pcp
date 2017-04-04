/*
 * List Windows performance counters on the current platform.
 *
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <stdio.h>
#include <stdlib.h>

static int verbose;
extern char *pdherrstr(int);
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))

void
expand(char *pat)
{
    LPTSTR		ptr;
    LPSTR		buf = NULL;
    DWORD		bufsz = 0;
    int			i;
    PDH_STATUS  	pdhsts;

    // Iterate because size grows in first couple of attempts!
    for (i = 0; i < 5; i++) {
	if (bufsz && (buf = (LPSTR)malloc(bufsz)) == NULL) {
	    fprintf(stderr, "malloc %ld failed for pattern: %s\n", bufsz, pat);
	    return;
	}
	if (verbose)
	    fprintf(stderr, "ExpandCounters pattern: %s\n", pat);
	if ((pdhsts = PdhExpandCounterPathA(pat, buf, &bufsz)) == PDH_MORE_DATA) {
	    // bufsz has the required length (minus the last NULL)
	    bufsz = roundup(bufsz + 1, 64);
	    free(buf);
	}
	else
	    break;
    }

    if (pdhsts == PDH_CSTATUS_VALID_DATA) {
	// success, print all counters
	ptr = buf;
	while (*ptr) {
	    printf("%s\n", ptr);
	    ptr += strlen(ptr) + 1;
	}
    }
    else {
	fprintf(stderr, "PdhExpandCounterPathA failed: %s\n", pdherrstr(pdhsts));
	if (pdhsts == PDH_MORE_DATA)
	    fprintf(stderr, "still need to resize buffer to %ld\n", bufsz);
    }

    fflush(stderr);
    fflush(stdin);
}

int
main(int argc, char **argv)
{
    int	i;

    if (argc == 1) {
	expand("\\*\\*");
	expand("\\*(*)\\*");
	return 0;
    }

    for (i = 1; i < argc; i++)
	expand(argv[i]);
    return 0;
}
