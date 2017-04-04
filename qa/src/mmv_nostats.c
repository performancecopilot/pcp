/*
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

int 
main(int ac, char * av[])
{
    char * file = (ac > 1) ? av[1] : "nostats";
    void * addr = mmv_stats_init(file, 0, 0,
			NULL, 0, NULL, 0);

    if (!addr) {
	fprintf(stderr, "mmv_stats_init failed : %s\n", strerror(errno));
	return 1;
    }

    mmv_stats_add(addr, "counter", "", 40);

    return 0;
}
