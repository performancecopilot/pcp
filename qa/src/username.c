/*
 * Copyright (c) 2012 Red Hat.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <unistd.h>

static void
usage (void)
{
    fprintf(stderr, "Usage %s: username\n", pmProgname);
    exit(1);
}

int
main(int argc, char* argv[])
{
    int sts;

    __pmSetProgname(argv[0]);
    if (argc != 2)
	usage();
    sts = __pmSetProcessIdentity(argv[1]);
    pause();
    return sts;
}
