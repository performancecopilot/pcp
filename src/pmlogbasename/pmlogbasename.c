/*
 * Copyright (c) 2024 Ken McDonell.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "libpcp.h"
#include <ctype.h>

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "filename",
};

int
main(int argc, char **argv)
{
    int		c;
    char	*res;
    const char	*sl;
    char	*s = NULL;
    char	*p;
    char	pathname[MAXPATHLEN];

    /*
     * only possible command line option is -D
     */
    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }
    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT) || opts.optind == argc) {
	pmUsageMessage(&opts);
	exit(1);
    }

    /*
     * if filename ends with .<digits> then we need to probe the
     * filesystem here to disambiguate filenames like foobar.27 where
     * foobar could be the basename and 27 is the archive volume
     * number, or foobar.27 could be the basename ... in the latter
     * case foobar.27.meta should exist, although it may have been
     * compressed and have an additional suffix.
     */
    p = strrchr(argv[opts.optind], '.');
    if (p != NULL) {
	p++;
	if (isdigit(*p)) {
	    /* at least one digit after last . */
	    p++;
	    while (*p != '\0' && isdigit(*p))
		p++;
	    if (*p == '\0') {
		/* ends in .<digits> */
		pmsprintf(pathname, sizeof(pathname), "%s.meta", argv[opts.optind]);
		if (access(pathname, F_OK) == 0) {
		    if (pmDebugOptions.appl0)
			fprintf(stderr, "found %s\n", pathname);
		    goto nochange;
		}
		else if (pmDebugOptions.appl1)
		    fprintf(stderr, "probe %s\n", pathname);
		sl = pmGetAPIConfig("compress_suffixes");
		if (sl != NULL)
		    s = strdup(sl);
		/*
		 * s is a space separated list of suffixes, e.g.
		 * .xz .lzma .bz2 .bz .gz .Z .z .zst
		 */
		while (s != NULL && *s != '\0') {
		    p = strchr(s, ' ');
		    if (p != NULL)
			*p = '\0';
		    pmsprintf(pathname, sizeof(pathname), "%s.meta%s", argv[opts.optind], s);
		    if (access(pathname, F_OK) == 0) {
			if (pmDebugOptions.appl0)
			    fprintf(stderr, "found %s\n", pathname);
			goto nochange;
		    }
		    else if (pmDebugOptions.appl1)
			fprintf(stderr, "probe %s\n", pathname);
		    if (p == NULL)
			break;
		    s = ++p;
		}
	    }
	}
    }

    /*
     * OK, no trailing .<digits> and/or no matching .meta which means
     * filename is probably the name of a file, so off to strip suffixes.
     *
     * need strdup() 'cause __pmLogBaseName() clobbers the argument
     * in place
     */
    if ((res = strdup(argv[opts.optind])) == NULL) {
	fprintf(stderr, "pmlogbasename: strdup(%s) failed!\n", argv[opts.optind]);
	exit(1);
    }
    res = __pmLogBaseName(res);
    if (res == NULL)
	goto nochange;
    else
	printf("%s\n", res);
    return 0;

nochange:
    printf("%s\n", argv[opts.optind]);
    return 0;
}
