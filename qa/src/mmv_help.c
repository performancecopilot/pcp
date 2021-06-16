/*
 * Setup an mmv PMDA file to exercise various help text code paths.
 * Based on mmv3_genstats.c
 *
 * Copyright (c) 2018 Guillem Lopez Paradis. All Rights Reserved.
 * Copyright (c) 2021 Ken McDonell. All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

static mmv_metric2_t metrics[] = {
    {	.name = "qa.nohelp",
	.item = 1,
	.type = MMV_TYPE_U32,
	.semantics = MMV_SEM_DISCRETE,
	.dimension = MMV_UNITS(0,0,0,0,0,0),
	.shorttext = NULL,
	.helptext = NULL
    },
    {	.name = "qa.oneline",
	.item = 1,
	.type = MMV_TYPE_U32,
	.semantics = MMV_SEM_DISCRETE,
	.dimension = MMV_UNITS(0,0,0,0,0,0),
	.shorttext = "oneline",
	.helptext = NULL
    },
    {	.name = "qa.both",
	.item = 1,
	.type = MMV_TYPE_U32,
	.semantics = MMV_SEM_DISCRETE,
	.dimension = MMV_UNITS(0,0,0,0,0,0),
	.shorttext = "foo",
	.helptext = "bar"
    },
    {	.name = "qa.verbose",
	.item = 1,
	.type = MMV_TYPE_U32,
	.semantics = MMV_SEM_DISCRETE,
	.dimension = MMV_UNITS(0,0,0,0,0,0),
	.shorttext = "wordy",
	.helptext = "One, two, three, four, five,\n"
"Once I caught a fish alive,\n"
"Six, seven, eight, nine, ten,\n"
"Then I let go again.\n"
"\n"
"Why did you let it go?\n"
"Because it bit my finger so.\n"
"Which finger did it bite?\n"
"This little finger on the right"
    },
    {	.name = "qa.help_empty",
	.item = 1,
	.type = MMV_TYPE_U32,
	.semantics = MMV_SEM_DISCRETE,
	.dimension = MMV_UNITS(0,0,0,0,0,0),
	.shorttext = "foobar",
	.helptext = ""
    },
    {	.name = "qa.both_empty",
	.item = 1,
	.type = MMV_TYPE_U32,
	.semantics = MMV_SEM_DISCRETE,
	.dimension = MMV_UNITS(0,0,0,0,0,0),
	.shorttext = "",
	.helptext = ""
    },
};

    int 
main(int ac, char * av[])
{
    int i;
    void *addr;
    char * file = (ac > 1) ? av[1] : "mmv_help";
    /* choose cluster to avoid collisions, 322 seems safe */
    mmv_registry_t	*registry = mmv_stats_registry(file, 322, 0);

    if (!registry) {
	fprintf(stderr, "mmv_stats_registry: %s - %s\n", file, strerror(errno));
	return 1;
    }

    for (i = 0; i < sizeof(metrics) / sizeof(metrics[0]); i++) {
	mmv_stats_add_metric(registry,
			 metrics[i].name, metrics[i].item, metrics[i].type,
			 metrics[i].semantics, metrics[i].dimension, metrics[i].indom,
			 metrics[i].shorttext, metrics[i].helptext);
    }

    addr = mmv_stats_start(registry);

    if (!addr) {
	fprintf(stderr, "mmv_stats_start: %s - %s\n", file, strerror(errno));
	return 1;
    }

    return 0;
}
