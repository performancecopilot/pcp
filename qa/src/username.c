/*
 * Copyright (c) 2012 Red Hat.
 */

#include <pcp/pmapi.h>
#include <unistd.h>

#include "localconfig.h"

#if PCP_VER < 3611
#define pmSetProcessIdentity(x) (exit(1), 1)
#endif

static void
usage (void)
{
    fprintf(stderr, "Usage %s: username\n", pmGetProgname());
    exit(1);
}

int
main(int argc, char* argv[])
{
    int sts;

    pmSetProgname(argv[0]);
    if (argc != 2)
	usage();
    sts = pmSetProcessIdentity(argv[1]);
#ifndef IS_MINGW
    pause();
#else
    /*
     * punt ... looks like a signal will get us back from here
     * (at least it works in one test case with SIGINT)
     */
    SleepEx(INFINITE, TRUE);
#endif
    return sts;
}
