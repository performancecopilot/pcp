/***********************************************************************
 * pmie_dump_stats - dump "stats" file that sits between pmie and the
 * pmcd PMDA ... designed to be used by pmiectl
 ***********************************************************************
 *
 * Copyright (c) 2020 Ken McDonell.  All Rights Reserved.
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
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "pmapi.h"
#include "libpcp.h"
#include "./stats.h"

int
main(int argc, char **argv)
{
    int		fd;
    int		sts;
    pmiestats_t	stats;

    if (argc < 2) {
	fprintf(stderr, "Usage: pmie_dump_stats file ...\n");
	return 1;
    }

    while (argc >= 2) {
	if ((fd = open(argv[1], O_RDONLY)) < 0) {
	    fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
	    return 1;
	}
	if ((sts = read(fd, &stats, sizeof(stats))) != sizeof(stats)) {
	    fprintf(stderr, "%s: read %d != %d as expected\n", argv[1], sts, (int)sizeof(stats));
	}
	else {
	    char	*p;
	    p = strrchr(argv[1], pmPathSeparator());
	    if (p == NULL)
		p = argv[1];
	    else
		p++;
	    /*
	     * Ignore Coverity errors here ... these strings are all
	     * guaranteed to be null-byte terminated in the binary pmie
	     * stats file.
	     */
	    /* coverity[string_null] */
	    printf("%s:config=%s\n", p, stats.config);
	    /* coverity[string_null] */
	    printf("%s:logfile=%s\n", p, stats.logfile);
	    /* coverity[string_null] */
	    printf("%s:pmcd_host=%s\n", p, stats.defaultfqdn);

	    printf("%s:eval_expected=%.2f\n", p, stats.eval_expected);
	    printf("%s:numrules=%d\n", p, stats.numrules);
	    printf("%s:actions=%d\n", p, stats.actions);
	    printf("%s:eval_true=%d\n", p, stats.eval_true);
	    printf("%s:eval_false=%d\n", p, stats.eval_false);
	    printf("%s:eval_unknown=%d\n", p, stats.eval_unknown);
	    printf("%s:eval_actual=%d\n", p, stats.eval_actual);
	    printf("%s:version=%d\n", p, stats.version);
	}
	argc--;
	argv++;
    }

    return 0;
}
