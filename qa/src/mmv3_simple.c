/* C language writer - using the application-level API, MMV v3 */
/* Build via: cc -g -Wall -lpcp_mmv -o mmv3_simple mm3v_simple.c */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

static mmv_metric2_t metrics[] = {
    {   .name = "simple3.u32.counter",
        .item = 1,
        .type = MMV_TYPE_U32,
        .semantics = MMV_SEM_COUNTER,   
        .dimension = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE),
        .shorttext = "metric shortext1",
        .helptext = "metric helptext1",
    },
    {   .name = "simple3.u64.instant",
        .item = 2,
        .type = MMV_TYPE_U64,
        .semantics = MMV_SEM_INSTANT,
        .dimension = MMV_UNITS(0,0,0,0,0,0),
        .shorttext = "metric shorttext2",
        .helptext = "metric helptext2",
    },
};

int
main(int argc, char **argv)
{
    int			i;
    void		*map;
    pmAtomValue		*value;
    char		*file = (argc > 1) ? argv[1] : "simple3";
    mmv_registry_t	*registry = mmv_stats_registry(file, 321, 0);

    if (!registry) {
	fprintf(stderr, "mmv_stats_registry: %s - %s\n", file, strerror(errno));
	return 1;
    }

    for (i = 0; i < sizeof(metrics) / sizeof(mmv_metric2_t); i++)
	mmv_stats_add_metric(registry,
			 metrics[i].name, metrics[i].item, metrics[i].type,
			 metrics[i].semantics, metrics[i].dimension, 0,
			 metrics[i].shorttext, metrics[i].helptext);
   
    mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string\"", MMV_STRING_TYPE, 0);
    mmv_stats_add_metric_label(registry, 1,
		    "metric_label", "321", MMV_NUMBER_TYPE, 0);

    map = mmv_stats_start(registry);
    if (!map) {
	fprintf(stderr, "mmv_stats_start: %s - %s\n", file, strerror(errno));
	return 1;
    }

    value = mmv_lookup_value_desc(map, metrics[0].name, NULL);
    mmv_inc_value(map, value, 42);
    mmv_stats_free(registry);
    return 0;
}
