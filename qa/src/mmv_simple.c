/* C language writer - using the application-level API */
/* Build via: cc -g -Wall -lpcp_mmv -o mmv_simple mmv_simple.c */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

static mmv_metric_t metrics[] = {
    {	.name = "simple.counter",
	.item = 1,
	.type = MMV_TYPE_U32,
	.semantics = MMV_SEM_COUNTER,
	.dimension = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE),
	.shorttext = NULL,
	.helptext = NULL,
    },
};

int 
main(int argc, char **argv)
{
    pmAtomValue *value;
    char *file = (argc > 1) ? argv[1] : "simple";
    void *addr = mmv_stats_init(file, 123, 0, metrics, 1, NULL, 0);

    if (!addr) {
	fprintf(stderr, "mmv_stats_init: %s - %s\n", file, strerror(errno));
	return 1;
    }

    value = mmv_lookup_value_desc(addr, "simple.counter", NULL);
    mmv_inc_value(addr, value, 42);

    mmv_stats_stop(file, addr);
    return 0;
}
