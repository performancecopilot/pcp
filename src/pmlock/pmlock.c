/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "pmapi.h"

int
main(int argc, char **argv)
{
    int		verbose = 0;
    extern int	errno;

    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
	verbose = 1;
	argc--;
	argv++;
    }
    if (argc != 2 || (argc == 2 && strcmp(argv[1], "-?") == 0)) {
	fprintf(stderr, "Usage: pmlock [-v] file\n");
	exit(1);
    }
    if (open(argv[1], O_CREAT|O_EXCL|O_RDONLY, 0) < 0) {
	if (verbose) {
	    if (errno == EACCES) {
		char	*p = dirname(argv[1]);
		if (access(p, W_OK) == -1)
		    printf("%s: Directory not writeable\n", p);
		else
		    printf("%s: %s\n", argv[1], strerror(EACCES));
	    }
	    else
		printf("%s: %s\n", argv[1], strerror(errno));
	}
	exit(1);
    }
	
    exit(0);
}
