/*
 * Copyright (c) 2017 Red Hat.
 *
 * Convert raw integers into dotted-form InDoms.
 */
#include <pcp/pmapi.h>

int
main(int argc, char *argv[])
{
    pmInDom	indom;

    while (optind < argc) {
	indom = atoi(argv[optind++]);
	printf("%u = %s\n", indom, pmInDomStr(indom));
    }
    return 0;
}
