/*
 * Copyright (c) 2010 Max Matveev.  All Rights Reserved.
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
#include <sys/stat.h>
#include "pmapi.h"
#include "libpcp.h"
#include "stats.h"

int
main(int argc, char **argv)
{
    int i;
    pmSetProgname(argv[0]);

    for (i=1; i < argc; i++) {
	pmiestats_t *pp = NULL;
	struct stat st;
	int f = open(argv[i], O_RDONLY, 0);

	if (i == 1 && strcmp(argv[i], "-v") == 0) {
	    fprintf(stderr, "pmiestats_t: %d bytes, version offset: %d\n",
		(int)sizeof(pmiestats_t), (int)((char *)&pp->version - (char *)pp));
	    continue;
	}

	if (f < 0) {
	    fprintf(stderr, "%s: cannot open %s - %s\n",
		    pmGetProgname(), argv[i], osstrerror());
	    continue;
	}

	if (fstat(f, &st) < 0) {
	    fprintf(stderr, "%s: cannot get size of %s - %s\n",
		    pmGetProgname(), argv[i], osstrerror());
	    goto closefile;
	}

	if (st.st_size != sizeof(pmiestats_t)) {
	    fprintf(stderr, "%s: %s is not a valid pmie stats file\n",
		    pmGetProgname(), argv[i]);
	    goto closefile;
	}
	if ((pp = __pmMemoryMap(f, st.st_size, 0)) == NULL) {
	    fprintf(stderr, "%s: __pmMemoryMap failed for %s: %s\n",
		    pmGetProgname(), argv[i], osstrerror());
	    goto closefile;
	}

	if (pp->version != 1) {
	    fprintf(stderr, "%s: unsupported version %d in %s\n",
		    pmGetProgname(), pp->version, argv[i]);
	    goto closefile;
	}

	printf ("%s\n%s\n%s\n", pp->config, pp->logfile, pp->defaultfqdn);
closefile:
	close(f);
	if (pp != NULL) {
	    __pmMemoryUnmap(pp, st.st_size);
	    pp = NULL;
	}
    }
    return 0;
}
