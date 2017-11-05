/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/deprecated.h>

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
    int		sts;

    printf("pmGetProgname=\"%s\"\n", pmGetProgname());

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    printf("pmGetProgname()=\"%s\"\n", pmGetProgname());
    printf("pmProgname=\"%s\"\n", pmProgname);

    sts = __pmSetProgname("foo");
    printf("__pmSetProgname(foo) -> %d & pmProgname=\"%s\"\n", sts, pmProgname);
    printf("pmGetProgname()=\"%s\"\n", pmGetProgname());

    pmSetProgname("bar");
    printf("pmSetProgname(bar) & pmGetProgname()=\"%s\"\n", pmGetProgname());
    printf("pmProgname=\"%s\"\n", pmProgname);

    pmSetProgname(NULL);
    printf("pmSetProgname(NULL) & pmGetProgname()=\"%s\"\n", pmGetProgname());
    printf("pmProgname=\"%s\"\n", pmProgname);

    return 0;
}
