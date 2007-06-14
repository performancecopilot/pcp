/*
 * List all Windows performance counters on the current platform
 *
 * Expected running mode is ./show-all-ctrs | sort >some file
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Contact information: Ken McDonell, kenj At internode DoT on DoT net
 */

#include <pdh.h>
#include <pdhmsg.h>
#include <stdio.h>

extern char *pdherrstr(int);

void
_do(char *pat)
{
    LPSTR		buf;
    int			bufsz;
    int			i;
    PDH_STATUS  	pdhsts;
    static HQUERY 	hQuery = NULL;

    if (hQuery == NULL) {
	pdhsts = PdhOpenQuery(0,0, &hQuery);
	if (pdhsts != ERROR_SUCCESS) {
	    fprintf(stderr, "PdhOpenQuery failed: %s\n", pdherrstr(pdhsts));
	    return;
	}
    }

    // Initial buffer size is 100% guess
    bufsz = 100000;

    // iterate because size grows in first couple of attempts!
    for (i = 0; i < 5; i++) {
	if ((buf = (LPSTR)malloc(bufsz)) == NULL) {
	    fprintf(stderr, "Arrgh ... malloc %d failed for pattern %s\n", bufsz, pat);
	    return;
	}
	if ((pdhsts = PdhExpandCounterPath(pat, buf, &bufsz)) == PDH_MORE_DATA) {
	    // bufsz has the required length (minus the last NULL)
	    bufsz++;
	    free(buf);
	}
	else
	    break;
    }

    if (pdhsts == PDH_CSTATUS_VALID_DATA) {
	// success, print all counters
	LPTSTR ptr;

	ptr = buf;
	while (*ptr) {
	    printf("%s\n", ptr);
	    ptr += strlen(ptr)+1;
	}
    }
    else {
	fprintf(stderr, "PdhExpandCounterPath failed: %s\n", pdherrstr(pdhsts));
	if (pdhsts == PDH_MORE_DATA)
	    fprintf(stderr, "still need to resize buffer to %d\n", bufsz);
    }
}

void
main(int argc, char **argv)
{
    // Forms we're looking for ...
    //    \object\counter
    //    \object(parent/instance#index)\counter
    //

    _do("\\*\\*");
    _do("\\*(*/*#*)\\*");
}
