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
 */

#include "pmapi.h"
#include <fcntl.h>
#include <sys/stat.h>

int
main(int argc, char **argv)
{
    int		fd, verbose = 0;

    if (argc > 1 &&
	(strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--verbose") == 0)) {
	verbose = 1;
	argc--;
	argv++;
    }
    if (argc != 2 || (argc == 2 && strcmp(argv[1], "-?") == 0)) {
	fprintf(stderr, "Usage: pmlock [-v,--verbose] file\n");
	exit(1);
    }
    if ((fd = open(argv[1], O_CREAT|O_EXCL|O_RDONLY, 0)) < 0) {
	if (verbose) {
	    if (oserror() == EACCES) {
		char	*p = dirname(argv[1]);
		if (access(p, W_OK) == -1)
		    printf("%s: Directory not writable\n", p);
		else
		    printf("%s: %s\n", argv[1], strerror(EACCES));
	    }
	    else
		printf("%s: %s\n", argv[1], osstrerror());
	}
	exit(1);
    }
    close(fd);
    exit(0);
}
