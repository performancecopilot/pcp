/* C language writer - using the application-level API, MMV v3 */
/* Build via: cc -g -Wall -lpcp_mmv -o mmv3_labels mm3v_labels.c */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

static mmv_metric2_t metrics[] = {
    {   .name = "labels3.u32.counter",
        .item = 1,
	.indom = 1,
        .type = MMV_TYPE_U32,
        .semantics = MMV_SEM_COUNTER,   
        .dimension = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE),
        .shorttext = "metric shortext1",
        .helptext = "metric helptext1",
    },
    {   .name = "labels3.u64.instant",
        .item = 2,
	.indom = 2,
        .type = MMV_TYPE_U64,
        .semantics = MMV_SEM_INSTANT,
        .dimension = MMV_UNITS(0,0,0,0,0,0),
        .shorttext = "metric shorttext2",
        .helptext = "metric helptext2",
    },
};

static mmv_instances2_t cpus[] = {
    {  0, "cpu0" },
    {  1, "cpu1" },
    {  2, "cpu3" },
};

static mmv_instances2_t disk[] = {
    {  .internal = 0,
       .external = "sda" },
    {  .internal = 1,
       .external = "sdb" },
};

static mmv_indom2_t indoms[] = {
    {	.serial = 1,
	.instances = cpus,
	.count = (sizeof(cpus) / sizeof(mmv_instances2_t)),
	.shorttext = "indom shorttext1",
	.helptext = "indom helptext1",
    },
    {	.serial = 2,
	.instances = disk,
	.count = (sizeof(disk) / sizeof(mmv_instances2_t)),
	.shorttext = "indom shorttext2",
	.helptext = "indom helptext2",
    },
};

int
main(int argc, char **argv)
{
    int			i;
    void		*map;
    pmAtomValue		*value;
    char		*file = (argc > 1) ? argv[1] : "labels3";
    mmv_registry_t	*registry = mmv_stats_registry(file, 322, 0);

    if (!registry) {
	fprintf(stderr, "mmv_stats_registry: %s - %s\n", file, strerror(errno));
	return 1;
    }

    for (i = 0; i < sizeof(metrics) / sizeof(mmv_metric2_t); i++)
	mmv_stats_add_metric(registry,
			 metrics[i].name, metrics[i].item, metrics[i].type,
			 metrics[i].semantics, metrics[i].dimension, metrics[i].indom,
			 metrics[i].shorttext, metrics[i].helptext);
   
    for (i = 0; i < sizeof(indoms) / sizeof(mmv_indom2_t); i++)
	mmv_stats_add_indom(registry,
			indoms[i].serial,
			indoms[i].shorttext, indoms[i].helptext);

    for (i = 0; i < sizeof(disk) / sizeof(mmv_instances2_t); i++)
	mmv_stats_add_instance(registry,
			indoms[1].serial,
			disk[i].internal, disk[i].external);

    for (i = 0; i < sizeof(cpus) / sizeof(mmv_instances2_t); i++)
	mmv_stats_add_instance(registry,
			indoms[0].serial,
			cpus[i].internal, cpus[i].external);

    mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string\"", MMV_STRING_TYPE, 0);
    mmv_stats_add_metric_label(registry, 1,
		    "metric_label", "null", MMV_NULL_TYPE, 1);
    mmv_stats_add_indom_label(registry, 1,
		    "indom_label", "42.001", MMV_NUMBER_TYPE, 0);
    mmv_stats_add_instance_label(registry, 1, 0,
		    "item_label1", "true", MMV_BOOLEAN_TYPE, 1);
    mmv_stats_add_instance_label(registry, 1, 1,
		    "item_label2", "[1,2,3,4,5]", MMV_ARRAY_TYPE, 0);
    mmv_stats_add_instance_label(registry, 1, 2,
		    "item_label3", "{\"a\":1,\"b\":2}", MMV_MAP_TYPE, 1);

    map = mmv_stats_start(registry);
    if (!map) {
	fprintf(stderr, "mmv_stats_start: %s - %s\n", file, strerror(errno));
	mmv_stats_free(registry);
	return 1;
    }

    value = mmv_lookup_value_desc(map, "labels3.u32.counter", "cpu0");
    mmv_inc_value(map, value, 42);
    mmv_stats_free(registry);
    return 0;
}
