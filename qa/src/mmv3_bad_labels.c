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
check_label(const char *name, const char *value, mmv_value_type_t type, int flags) {
    pmLabelSet *set = NULL;
    int len, sts;
    char buffer[244];
    double aux = 1.0;
    char *endnum;

    /* The +5 is for the characters we add next - {"":} */
    if (strlen(name) + strlen(value) + 5 > MMV_LABELMAX) {
	setoserror(E2BIG);
	return -1;
    }

    switch (type) {
    	case MMV_NULL_TYPE:
	    value = "null";
	    break;
    	case MMV_BOOLEAN_TYPE:
	    if (!strcmp(value,"true") && !strcmp(value,"false")) {
		setoserror(EINVAL);
		return -1;
	    }
	    break;
    	case MMV_MAP_TYPE:
	    if (value[0] != '{' ||  value[strlen(value)-1] != '}') {
		setoserror(EINVAL);
		return -1;
	    }
            break;
	case MMV_ARRAY_TYPE:
	    if (value[0] != '[' ||  value[strlen(value)-1] != ']') {
		setoserror(EINVAL);
		return -1;
	    }
            break;
    	case MMV_STRING_TYPE:
	    if (value[0] != '\"' ||  value[strlen(value)-1] != '\"') {
		setoserror(EINVAL);
		return -1;
	    }
	    break;
 	case MMV_NUMBER_TYPE:
	    aux = strtod(value, &endnum);
            if (*endnum != '\0') {
		setoserror(EINVAL);
		return -1;
            }
	    break;
    	default:
	    setoserror(EINVAL);
	    return -1; // error
    }

    
    len = pmsprintf(buffer, MMV_LABELMAX, "{\"%s\":%s}", name, value);
    
    if ((sts = __pmParseLabelSet(buffer, len, flags, &set)) < 0) {
	setoserror(sts);
	return -1;
    }
    pmFreeLabelSets(set, 1);
    return len;
}

int
main(int argc, char **argv)
{
    int			i, cnt, ret;
    void		*map;
    char		*file = (argc > 1) ? argv[1] : "bad_labels";
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
   
    
    ret = mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string1\"", MMV_STRING_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "string2\"", MMV_STRING_TYPE, 0);
    
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string3", MMV_STRING_TYPE, 0);
    
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"stri\"n\"g4\"", MMV_STRING_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"stri\"", MMV_STRING_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"true\"", MMV_BOOLEAN_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "True", MMV_BOOLEAN_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "False", MMV_BOOLEAN_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"false\"", MMV_BOOLEAN_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "2.0", MMV_NUMBER_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "2.2", MMV_NUMBER_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "1", MMV_NUMBER_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "-12", MMV_NUMBER_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"12\"", MMV_NUMBER_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "12.13.13", MMV_NUMBER_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "12.13.13", MMV_NULL_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "null", MMV_NULL_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[1,2,3]", MMV_ARRAY_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "1,2,3]", MMV_ARRAY_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[1,2,3", MMV_ARRAY_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[1,2 3]", MMV_ARRAY_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[\"123\",12]", MMV_ARRAY_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{\"a\":1,\"b\":2}", MMV_MAP_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"a\":1,\"b\":2}", MMV_MAP_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{\"a\":1,\"b\":2", MMV_MAP_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{a\":1,\"b\":2}", MMV_MAP_TYPE, 0);

    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{a:1,\"b\":2}", MMV_MAP_TYPE, 0);

    map = mmv_stats_start(registry);
    
    if (!map) {
	fprintf(stderr, "mmv_stats_start: %s - %s\n", file, strerror(errno));
	return 1;
    }

    mmv_stats_free(registry);
    return 0;
}
