/*
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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
 */

/*
 * Parse a command line string that encodes arguments to __pmLocalPMDA(),
 * then call __pmLocalPMDA().
 *
 * The syntax for the string is 1 to 4 fields separated by colons:
 * 	- op ("add" for add, "del" for delete, "clear" for clear)
 *	- domain (PMDA's PMD)
 *	- path (path to DSO PMDA)
 *	- init (name of DSO's initialization routine)
 */

#include "pmapi.h"
#include "impl.h"

int
do_local_spec(const char *spec)
{
    int		op;
    int		domain = -1;
    char	*name = NULL;
    char	*init = NULL;
    int		sts;
    char	*arg;
    char	*ap;

    if ((arg = strdup(spec)) == NULL) {
	fprintf(stderr, "%s: cannot dup spec \"%s\": %s\n", pmProgname, spec, pmErrStr(-errno));
	return 1;
    }
    if (strncmp(arg, "add", 3) == 0) {
	op = PM_LOCAL_ADD;
	ap = &arg[3];
    }
    else if (strncmp(arg, "del", 3) == 0) {
	op = PM_LOCAL_DEL;
	ap = &arg[3];
    }
    else if (strncmp(arg, "clear", 5) == 0) {
	op = PM_LOCAL_CLEAR;
	ap = &arg[5];
    }
    else {
	fprintf(stderr, "%s: bad op in spec \"%s\"\n", pmProgname, spec);
	return 1;
    }
    if (op == PM_LOCAL_CLEAR && *ap == '\0')
	goto doit;

    if (*ap != ',') {
	fprintf(stderr, "%s: bad spec \"%s\"\n", pmProgname, spec);
	return 1;
    }
    arg = ++ap;
    if (*ap != ',' && *ap != '\0') {
	domain = (int)strtol(arg, &ap, 10);
	if ((*ap != ',' && *ap != '\0') || domain < 0 || domain > 510) {
	    fprintf(stderr, "%s: bad domain (%d) in spec \"%s\"\n", pmProgname, domain, spec);
	    return 1;
	}
    }
    if (*ap == ',') {
	ap++;
	if (*ap == ',') {
	    // no name, could have init (not useful but possible!)
	    ap++;
	    if (*ap != '\0')
		init = ap;
	}
	else if (*ap != '\0') {
	    // have name and possibly init
	    name = ap;
	    while (*ap != ',' && *ap != '\0')
		ap++;
	    if (*ap == ',') {
		*ap++ = '\0';
		if (*ap != '\0')
		    init = ap;
	    }
	}
    }

doit:
    sts = __pmLocalPMDA(op, domain, name, init);
    if (sts < 0) {
	fprintf(stderr, "%s: __pmAddLocalPMDA failed: %s\n", pmProgname, pmErrStr(sts));
	return 1;
    }
    return 0;
}
