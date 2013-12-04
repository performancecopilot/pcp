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
#include "impl.h"
#include "stats.h"

int
main(int argc, char **argv)
{
    int i;
    __pmSetProgname(argv[0]);

    for (i=1; i < argc; i++) {
	pmiestats_t ps;
	struct stat st;
	int f = open(argv[i], O_RDONLY, 0);

	if (f < 0) {
	    fprintf(stderr, "%s: cannot open %s - %s\n",
		    pmProgname, argv[i], osstrerror());
	    continue;
	}

	if (fstat(f, &st) < 0) {
	    fprintf(stderr, "%s: cannot get size of %s - %s\n",
		    pmProgname, argv[i], osstrerror());
	    goto closefile;
	}

	if (st.st_size != sizeof(ps)) {
	    fprintf(stderr, "%s: %s is not a valid pmie stats file\n",
		    pmProgname, argv[i]);
	    goto closefile;
	}
	if (read(f, &ps, sizeof(ps)) != sizeof(ps)) {
	    fprintf(stderr, "%s: cannot read %ld bytes from %s\n",
		    pmProgname, (long)sizeof(ps), argv[i]);
	    goto closefile;
	}

	if (ps.version != 1) {
	    fprintf(stderr, "%s: unsupported version %d in %s\n",
		    pmProgname, ps.version, argv[i]);
	    goto closefile;
	}

	printf ("%s\n%s\n%s\n", ps.config, ps.logfile, ps.defaultfqdn);
closefile:
	close(f);
    }
    return 0;
}
