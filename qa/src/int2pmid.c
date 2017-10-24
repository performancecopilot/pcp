/*
 * Copyright (c) 2017 Red Hat.
 *
 * Convert raw integers into dotted-form PMIDs.
 */
#include <pcp/pmapi.h>

int
main(int argc, char *argv[])
{
    pmID	pmid;

    while (optind < argc) {
	pmid = atoi(argv[optind++]);
	printf("%u = %s\n", pmid, pmIDStr(pmid));
    }
    return 0;
}
