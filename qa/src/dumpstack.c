/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2022 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,	/* -D */
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:",
    .long_options = longopts,
    .short_usage = "[options] ...",
};

void
need_to_push_the_function_name_beyond_128_bytes_so_that_we_have_a_very_long_function_name_that_may_overflow_buffer_in__pmDumpStack(void)
{
    __pmDumpStack();
}


void
bar(void)
{
    need_to_push_the_function_name_beyond_128_bytes_so_that_we_have_a_very_long_function_name_that_may_overflow_buffer_in__pmDumpStack();
}

void
foo(void)
{
    bar();
}

void
a(void)
{
    __pmDumpStack();
    foo();
}

int
main(int argc, char **argv)
{
    int		c;
#ifdef HAVE___EXECUTABLE_START
    extern char	__executable_start;
#endif


    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors || opts.optind < argc) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

#ifdef HAVE___EXECUTABLE_START
    __pmDumpStackInit((void *)&__executable_start);
#endif

    a();

    return 0;
}

