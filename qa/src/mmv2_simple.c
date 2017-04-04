/* C language writer - using the application-level API, MMV v2 */
/* Build via: cc -g -Wall -lpcp_mmv -o mmv_simple mmv_simple.c */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

static mmv_metric2_t metrics[] = {
    {	.name = "simple2.counter",
	.item = 1,
	.type = MMV_TYPE_U32,
	.semantics = MMV_SEM_COUNTER,
	.dimension = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE),
	.shorttext = NULL,
	.helptext = NULL,
    },
    {	.name =
	"simple2.metric.with.a.much.longer.metric.name.forcing.version2.format",
	.item = 1,
	.type = MMV_TYPE_U64,
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
    char *file = (argc > 1) ? argv[1] : "simple2";
    void *addr = mmv_stats2_init(file, 321, 0, metrics, 2, NULL, 0);

    if (!addr) {
	fprintf(stderr, "mmv_stats2_init: %s - %s\n", file, strerror(errno));
	return 1;
    }

    value = mmv_lookup_value_desc(addr, "simple.counter", NULL);
    mmv_inc_value(addr, value, 42);

    mmv_stats_stop(file, addr);
    return 0;
}
