/*
 * Copyright (c) 2018 Guillem Lopez Paradis.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

int 
main(int argc, char **argv)
{
    void		*map;
    char		*file = (argc > 1) ? argv[1] : "mmv3_no_stats";
    mmv_registry_t	*registry = mmv_stats_registry(file, 321, 0);

    if (!registry) {
	fprintf(stderr, "mmv_stats_registry: %s - %s\n", file, strerror(errno));
	return 1;
    }

    map = mmv_stats_start(registry);
    if (!map) {
	fprintf(stderr, "mmv_stats_start: %s - %s\n", file, strerror(errno));
	return 1;
    }

    mmv_stats_add(map, "counter", "", 40);

    mmv_stats_free(registry);

    return 0;
}
