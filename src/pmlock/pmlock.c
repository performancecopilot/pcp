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

static pmLongOptions longopts[] = {
    { "ident", 1, 'i', "IDENT", "write identifying value into the lock file" },
    { "verbose", 0, 'v', "", "increase verbosity" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};
static pmOptions opts = {
    .short_options = "i:v?",
    .long_options = longopts,
    .short_usage = "[-v] [-i ident] file",
};

int
main(int argc, char **argv)
{
    int		fd, verbose = 0;
    int		c;
    int		mode = 0;
    char	*ident = NULL;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'v':	/* verbose */
	    verbose++;
	    break;

	case 'i':	/* ident */
	    ident = opts.optarg;
	    mode = 0444;
	    break;

	case '?':
	    pmUsageMessage(&opts);
	    exit(0);

	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors || opts.optind != argc - 1) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if ((fd = open(argv[opts.optind], O_CREAT|O_EXCL|O_WRONLY, mode)) < 0) {
	if (verbose) {
	    if (oserror() == EACCES) {
		char	*p = dirname(argv[opts.optind]);
		if (access(p, W_OK) == -1)
		    fprintf(stderr, "pmlock: %s: Directory not writable\n", p);
		else
		    fprintf(stderr, "pmlock: %s: %s\n", argv[opts.optind], strerror(EACCES));
	    }
	    else
		fprintf(stderr, "pmlock: %s: %s\n", argv[opts.optind], osstrerror());
	}
	exit(1);
    }

    if (ident != NULL) {
	int	len = strlen(ident);
	int	sts;

	if ((sts = write(fd, ident, len)) != len) {
	    fprintf(stderr, "pmlock: %s: Warning: failed to write ident (%s): %s\n",
		argv[opts.optind], ident, osstrerror());
	}
	if ((sts = write(fd, "\n", 1)) != 1) {
	    fprintf(stderr, "pmlock: %s: Warning: failed to append newline after ident (%s): %s\n",
		argv[opts.optind], ident, osstrerror());
	}
    }

    close(fd);
    exit(0);
}
