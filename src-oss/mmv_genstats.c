#include <pcp/mmv_stats.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

static mmv_stats_inst_t test_indom [] = {
    {  0, "zero" },
    {  1, "hero" },
    { -1, "" },
};

static mmv_stats_t metrics[] = {
    {	.name = "counter",
	.item = 1,
	.type = MMV_ENTRY_U32,
	.semantics = MMV_SEM_COUNTER,
	.dimension = PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE),
	.shorttext = "test counter metric",
	.helptext = "Yes, this is a test counter metric",
    },
    {	.name = "discrete",
	.item = 2,
	.type = MMV_ENTRY_I32,
	.semantics = MMV_SEM_DISCRETE,
	.dimension = PMDA_PMUNITS(0,0,0,0,0,0),
	.shorttext = "test discrete metric",
	.helptext = "Yes, this is a test discrete metric",
    },
    {	.name = "indom",
	.item = 3,
	.type = MMV_ENTRY_U32,	
	.semantics = MMV_SEM_INSTANT,
	.dimension = PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE),
	.indom = test_indom,
    },
    {	.name = "interval",
	.item = 4,
	.semantics = MMV_ENTRY_INTEGRAL,
	.semantics = MMV_SEM_COUNTER,
	.dimension = PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0),
    },
    {	.name = "string",
	.item = 5,
	.type = MMV_ENTRY_STRING,
	.dimension = PMDA_PMUNITS(0,0,0,0,0,0),
	.semantics = MMV_SEM_INSTANT,
    },
    {	.name = "strings",
	.item = 6,
	.type = MMV_ENTRY_STRING,
	.semantics = MMV_SEM_INSTANT,
	.dimension = PMDA_PMUNITS(0,0,0,0,0,0),
	.indom = test_indom,
	.shorttext = "test string metrics",
	.helptext = "Yes, this is a test string metric with instances",
    },
};

#define __METRIC_CNT (sizeof (metrics)/ sizeof (metrics[0]))

int 
main (int ac, char * av[])
{
    mmv_stats_value_t * vint = NULL;
    void * addr = mmv_stats_init (((ac>1)?av[1]:"test"), metrics, __METRIC_CNT);
    
    if (!addr) {
	printf ("mmv_stats_init failed : %s\n", strerror(errno));
	return 1;
    }

    /* start an interval */
    MMV_STATS_INTERVAL_START(addr,vint,interval,"");
    
    /* add ... */
    MMV_STATS_ADD (addr, counter, "", 41);
    /* add 1 ... */
    MMV_STATS_INC (addr, counter, "");

    /* set string values */
    MMV_STATS_SET_STRING (addr, string, "", "g'day world");
    MMV_STATS_SET_STRLEN (addr, strings, "zero", "00oo00oo00", 10);
    MMV_STATS_SET_STRLEN (addr, strings, "zero", "00oo00", 6);

    /* add (instance must be static) ... */
    MMV_STATS_STATIC_ADD (addr, discrete, "", 41);
    /* add 1 (instance must be static) ... */
    MMV_STATS_STATIC_INC (addr, discrete, "");

    /* add to instance or another if first doesn't exist */
    MMV_STATS_ADD_FALLBACK (addr, indom, "foobar", "unknown", 42);
    
    /* add to instance or another if first doesn't exist */
    MMV_STATS_ADD_FALLBACK (addr, indom, "zero", "unknown", 42);

    /* end an interval */
    MMV_STATS_INTERVAL_END(addr, vint);

    return 0;
}
