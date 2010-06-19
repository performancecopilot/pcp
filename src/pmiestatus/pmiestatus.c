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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pmapi.h"
#include "impl.h"
#include "pmiestats.h"

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
		    pmProgname, argv[i], strerror(errno));
	    continue;
	}

	if (fstat(f, &st) < 0) {
	    fprintf(stderr, "%s: cannot get size of %s - %s\n",
		    pmProgname, argv[i], strerror(errno));
	    continue;
	}

	if (st.st_size != sizeof(ps)) {
	    fprintf(stderr, "%s: %s is not a valid pmie stats file\n",
		    pmProgname, argv[i]);
	    continue;
	}
	if (read(f, &ps, sizeof(ps)) != sizeof(ps)) {
	    fprintf(stderr, "%s: cannot read %ld bytes from %s\n",
		    pmProgname, (long)sizeof(ps), argv[i]);
	    continue;
	}

	if (ps.version != 1) {
	    fprintf(stderr, "%s: unsupported version %d in %s\n",
		    pmProgname, ps.version, argv[i]);
	    continue;
	}

	printf ("%s\n%s\n%s\n", ps.config, ps.logfile, ps.defaultfqdn);
    }
    return 0;
}
