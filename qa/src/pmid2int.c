/*
 * Copyright (c) 2018 Red Hat.
 *
 * Convert dotted-form PMIDs into raw integers.
 */
#include <pcp/pmapi.h>

int
main(int argc, char *argv[])
{
    unsigned int cluster, domain, item;
    pmID pmid;

    while (optind < argc) {
	if (sscanf(argv[optind++], "%u.%u.%u", &domain, &cluster, &item) != 3)
	    continue;
	pmid = pmID_build(domain, cluster, item);
	printf("%u.%u.%u = %u\n", domain, cluster, item, pmid);
    }
    return 0;
}
