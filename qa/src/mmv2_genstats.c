/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
 * Copyright (c) 2016 Red Hat.
 */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

static mmv_instances2_t test_instances [] = {
    {  0, "zero" },
    {  1, "hero" },
};

static mmv_instances2_t nest_instances [] = {
    {  0, "bird" },
    {  1, "tree" },
    {  2, "eggs" },
};

static mmv_indom2_t indoms[] = {
    {	.serial = 1,
	.count = 2,
	.instances = test_instances,
	.shorttext = "We can be heroes",
	.helptext = "We can be heroes, just for one day",
    },
    {	.serial = 2,
	.count = 3,
	.instances = nest_instances,
	/* exercise no-help-text case */
    },
};

static mmv_metric2_t metrics[] = {
    {	.name = "counter",
	.item = 1,
	.type = MMV_TYPE_U32,
	.semantics = MMV_SEM_COUNTER,
	.dimension = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE),
	.shorttext = "test counter metric",
	.helptext = "Yes, this is a test counter metric",
    },
    {	.name = "discrete",
	.item = 2,
	.type = MMV_TYPE_I32,
	.semantics = MMV_SEM_DISCRETE,
	.dimension = MMV_UNITS(0,0,0,0,0,0),
	.shorttext = "test discrete metric",
	.helptext = "Yes, this is a test discrete metric",
    },
    {	.name = "indom",
	.item = 3,
	.type = MMV_TYPE_U32,	
	.semantics = MMV_SEM_INSTANT,
	.dimension = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE),
	.indom = 1,
	/* exercise no-help-text, no indom case */
    },
    {	.name = "interval",
	.item = 4,
	.type = MMV_TYPE_ELAPSED,
	.semantics = MMV_SEM_COUNTER,
	.dimension = MMV_UNITS(0,1,0,0,PM_TIME_USEC,0),
	.indom = 2,
	/* exercise no-help-text case */
    },
    {	.name = "string",
	.item = 5,
	.type = MMV_TYPE_STRING,
	.dimension = MMV_UNITS(0,0,0,0,0,0),
	.semantics = MMV_SEM_INSTANT,
	/* exercise no-help-text, string value case */
    },
    {	.name = "strings",
	.item = 6,
	.type = MMV_TYPE_STRING,
	.semantics = MMV_SEM_INSTANT,
	.dimension = MMV_UNITS(0,0,0,0,0,0),
	.indom = 1,
	.shorttext = "test string metrics",
	.helptext = "Yes, this is a test string metric with instances",
    },
};

static inline int indom_count() { return sizeof(indoms)/sizeof(indoms[0]); }
static inline int metric_count() { return sizeof(metrics)/sizeof(metrics[0]); }

int 
main(int ac, char * av[])
{
    pmAtomValue * atom;
    char * file = (ac > 1) ? av[1] : "test";
    int sleeper = (ac > 2) ? atoi(av[2]) : 0;
    void * addr = mmv_stats2_init(file, 0, 0,
			metrics, metric_count(), indoms, indom_count());

    if (!addr) {
	fprintf(stderr, "mmv_stats_init failed : %s\n", strerror(errno));
	return 1;
    }

    /* start an interval */
    atom = mmv_stats_interval_start(addr, NULL, "interval", "eggs");

    /* add ... */
    mmv_stats_add(addr, "counter", "", 40);
    /* add 1 ... */
    mmv_stats_inc(addr, "counter", "");

    /* set string values */
    mmv_stats_set_string(addr, "string", "", "g'day world");
    mmv_stats_set_strlen(addr, "strings", "zero", "00oo00oo00", 10);
    mmv_stats_set_strlen(addr, "strings", "zero", "00oo00oo00", 6);
    mmv_stats_set_strlen(addr, "strings", "hero", "ZERO", 4);
    mmv_stats_set_strlen(addr, "strings", "hero", "", 0);

    /* set discrete value ... */
    mmv_stats_set(addr, "discrete", "", 41);
    mmv_stats_inc(addr, "discrete", "");

    /* add to instance or another if first doesn't exist */
    mmv_stats_add_fallback(addr, "indom", "foobar", "unknown", 42);
    mmv_stats_add_fallback(addr, "indom", "zero", "unknown", 43);

    sleep(sleeper);

    /* end an interval */
    mmv_stats_interval_end(addr, atom);

    return 0;
}
