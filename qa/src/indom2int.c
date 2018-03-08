/*
 * Copyright (c) 2018 Red Hat.
 *
 * Convert dotted-form InDoms into raw integers.
 */
#include <pcp/pmapi.h>

int
main(int argc, char *argv[])
{
    unsigned int domain, serial;
    pmInDom indom;

    while (optind < argc) {
	if (sscanf(argv[optind++], "%u.%u", &domain, &serial) != 2)
	    continue;
	indom = pmInDom_build(domain, serial);
	printf("%u.%u = %u\n", domain, serial, indom);
    }
    return 0;
}
