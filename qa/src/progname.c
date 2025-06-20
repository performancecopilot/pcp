/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,		/* -D */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
    .short_options = "D:",
    .long_options = longopts,
    .short_usage = "[options]",
};

int
main(int argc, char **argv)
{
    int		c;

    printf("pmGetProgname()=\"%s\"\n", pmGetProgname());

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    printf("pmGetProgname()=\"%s\"\n", pmGetProgname());

    pmSetProgname("foo");
    printf("pmSetProgname(foo): pmGetProgname()=\"%s\"\n", pmGetProgname());

    pmSetProgname("bar");
    printf("pmSetProgname(bar): pmGetProgname()=\"%s\"\n", pmGetProgname());

    pmSetProgname(NULL);
    printf("pmSetProgname(NULL): pmGetProgname()=\"%s\"\n", pmGetProgname());

    return 0;
}
