#include <pcp/mmv_stats.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

static mmv_stats_inst_t test_indom [] = {
    {  0, "zero" },
    {  1, "unknown" },
    { -1, "" }
};

static mmv_stats_t metrics[] = {
    { "counter",	MMV_ENTRY_U32,		NULL,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE), MMV_SEM_COUNTER },
    { "discrete",	MMV_ENTRY_DISCRETE,	NULL,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE), MMV_SEM_DISCRETE },
    { "indom",		MMV_ENTRY_U32,		test_indom,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE), MMV_SEM_INSTANT },
    { "interval",	MMV_ENTRY_INTEGRAL,	NULL,
	PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0), MMV_SEM_COUNTER },
};

#define __METRIC_CNT (sizeof (metrics)/ sizeof (mmv_stats_metric_t))

int 
main (int ac, char * av[])
{
    mmv_stats_value_t * vint = NULL;
    void * addr = mmv_stats_init (((ac>1)?av[1]:"test"), metrics, __METRIC_CNT);
    
    if (!addr) {
	printf ("mmv_stats_init failed : %s\n", strerror(errno));
	printf ("(is \"%s\" in mmv.conf?)\n", ((ac>1)?av[1]:"test"));
	return 1;
    }

    /* start an interval */
    MMV_STATS_INTERVAL_START(addr,vint,interval,"");
    
    /* add ... */
    MMV_STATS_ADD (addr, counter, "", 41);
    /* add 1 ... */
    MMV_STATS_INC (addr, counter, "");
    
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
