/*
 * Check if getdomainname() returns something sane (when it is available).
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2018 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,		/* -D */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:",
    .long_options = longopts,
    .short_usage = "[options] ...",
};

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    char	buf[MAXDOMAINNAMELEN];

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	default:
		fprintf(stderr, "Eh? option %c\n", c);
		break;
	}
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

#ifdef HAVE_GETDOMAINNAME
    sts = getdomainname(buf, sizeof(buf));
    if (buf[0] == '\0')
	printf("sts=%d buf=<empty>\n", sts);
    else
	printf("sts=%d buf=\"%s\"\n", sts, buf);
#else
    printf("Don't have getdomainname() here.\n");
#endif

    return 0;
}

