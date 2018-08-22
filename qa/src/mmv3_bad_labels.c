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
    fprintf(stderr, "0 Return: %d, %s\n", ret, strerror(errno));


    fprintf(stderr, "STRING\n");

    // 1 Bad label string init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "string2\"", MMV_STRING_TYPE, 0);

    fprintf(stderr, "1 Return: %d, %s\n", ret, strerror(errno));

    errno = 0;
    
    // 2 Bad label string init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string3", MMV_STRING_TYPE, 0);

    fprintf(stderr, "2 Return: %d, %s\n", ret, strerror(errno));
    
    errno = 0;

    // 3 Bad label string init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"stri\"n\"g4\"", MMV_STRING_TYPE, 0);

    fprintf(stderr, "3 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 4 Bad label string init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"stri\"", MMV_STRING_TYPE, 0);

    fprintf(stderr, "4 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    fprintf(stderr, "BOOLEAN\n");

    // 5 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"true\"", MMV_BOOLEAN_TYPE, 0);

    fprintf(stderr, "5 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 6 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "True", MMV_BOOLEAN_TYPE, 0);

    fprintf(stderr, "6 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 7 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "False", MMV_BOOLEAN_TYPE, 0);

    fprintf(stderr, "7 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 8 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"false\"", MMV_BOOLEAN_TYPE, 0);

    fprintf(stderr, "8 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    fprintf(stderr, "NUMBER\n");

    // 9 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "2.0", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "9 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 10 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "2.2", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "10 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 11 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "1", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "11 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 12 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "-12", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "12 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 13 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"12\"", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "12 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 14 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "12.13.13", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "13 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    fprintf(stderr, "NULL\n");

    // 15 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "12.13.13", MMV_NULL_TYPE, 0);

    fprintf(stderr, "14 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 15 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "null", MMV_NULL_TYPE, 0);

    fprintf(stderr, "15 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    fprintf(stderr, "ARRAY\n");

    // 16 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[1,2,3]", MMV_ARRAY_TYPE, 0);

    fprintf(stderr, "16 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "1,2,3]", MMV_ARRAY_TYPE, 0);

    fprintf(stderr, "17 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[1,2,3", MMV_ARRAY_TYPE, 0);

    fprintf(stderr, "18 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[1,2 3]", MMV_ARRAY_TYPE, 0);

    fprintf(stderr, "19 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[\"123\",12]", MMV_ARRAY_TYPE, 0);

    fprintf(stderr, "20 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    fprintf(stderr, "MAP\n");

    // 16 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{\"a\":1,\"b\":2}", MMV_MAP_TYPE, 0);

    fprintf(stderr, "21 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"a\":1,\"b\":2}", MMV_MAP_TYPE, 0);

    fprintf(stderr, "22 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{\"a\":1,\"b\":2", MMV_MAP_TYPE, 0);

    fprintf(stderr, "23 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{a\":1,\"b\":2}", MMV_MAP_TYPE, 0);

    fprintf(stderr, "24 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{a:1,\"b\":2}", MMV_MAP_TYPE, 0);

    fprintf(stderr, "25 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    /*fprintf(stderr, "STRING\n");

    // 1 Bad label string init
    ret = check_label("label1", "string2\"", MMV_STRING_TYPE, 0);

    fprintf(stderr, "1 Return: %d, %s\n", ret, strerror(errno));

    errno = 0;
    
    // 2 Bad label string init
    ret = check_label("label1", "\"string3", MMV_STRING_TYPE, 0);

    fprintf(stderr, "2 Return: %d, %s\n", ret, strerror(errno));
    
    errno = 0;

    // 3 Bad label string init
    ret = check_label("label1", "\"stri\"n\"g4\"", MMV_STRING_TYPE, 0);

    fprintf(stderr, "3 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 4 Bad label string init
    ret = check_label("label1", "\"stri\"", MMV_STRING_TYPE, 0);

    fprintf(stderr, "4 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    fprintf(stderr, "BOOLEAN\n");

    // 5 Bad label boolean init
    ret = check_label("label1", "\"true\"", MMV_BOOLEAN_TYPE, 0);

    fprintf(stderr, "5 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 6 Bad label boolean init
    ret = check_label("label1", "True", MMV_BOOLEAN_TYPE, 0);

    fprintf(stderr, "6 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 7 Bad label boolean init
    ret = check_label("label1", "False", MMV_BOOLEAN_TYPE, 0);

    fprintf(stderr, "7 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 8 Bad label boolean init
    ret = check_label("label1", "\"false\"", MMV_BOOLEAN_TYPE, 0);

    fprintf(stderr, "8 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    fprintf(stderr, "NUMBER\n");

    // 9 Bad label boolean init
    ret = check_label("label1", "2.0", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "9 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 10 Bad label boolean init
    ret = check_label("label1", "2.2", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "10 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 11 Bad label boolean init
    ret = check_label("label1", "1", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "11 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 12 Bad label boolean init
    ret = check_label("label1", "-12", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "12 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 13 Bad label boolean init
    ret = check_label("label1", "\"12\"", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "12 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 14 Bad label boolean init
    ret = check_label("label1", "12.13.13", MMV_NUMBER_TYPE, 0);

    fprintf(stderr, "13 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    fprintf(stderr, "NULL\n");

    // 15 Bad label boolean init
    ret = check_label("label1", "12.13.13", MMV_NULL_TYPE, 0);

    fprintf(stderr, "14 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    // 15 Bad label boolean init
    ret = check_label("label1", "null", MMV_NULL_TYPE, 0);

    fprintf(stderr, "15 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    fprintf(stderr, "ARRAY\n");

    // 16 Bad label boolean init
    ret = check_label("label1", "[1,2,3]", MMV_ARRAY_TYPE, 0);

    fprintf(stderr, "16 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret = check_label("label1", "1,2,3]", MMV_ARRAY_TYPE, 0);

    fprintf(stderr, "17 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret = check_label("label1", "[1,2,3", MMV_ARRAY_TYPE, 0);

    fprintf(stderr, "18 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret = check_label("label1", "[1,2 3]", MMV_ARRAY_TYPE, 0);

    fprintf(stderr, "19 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret = check_label("label1", "[\"123\",12]", MMV_ARRAY_TYPE, 0);

    fprintf(stderr, "20 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

    fprintf(stderr, "MAP\n");

    // 16 Bad label boolean init
    ret = check_label("label1", "{\"a\":1,\"b\":2}", MMV_MAP_TYPE, 0);

    fprintf(stderr, "21 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret = check_label("label1", "\"a\":1,\"b\":2}", MMV_MAP_TYPE, 0);

    fprintf(stderr, "22 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret = check_label("label1", "{\"a\":1,\"b\":2", MMV_MAP_TYPE, 0);

    fprintf(stderr, "23 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret = check_label("label1", "{a\":1,\"b\":2}", MMV_MAP_TYPE, 0);

    fprintf(stderr, "24 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;

     // 16 Bad label boolean init
    ret = check_label("label1", "{a:1,\"b\":2}", MMV_MAP_TYPE, 0);

    fprintf(stderr, "25 Return: %d, %s\n", ret, strerror(errno));
     
    errno = 0;*/
    

    /*ret = mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string\"", MMV_STRING_TYPE, 0);
    fprintf(stderr, "Return: %d, %s\n", ret, strerror(errno));

    ret = mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string\"", MMV_STRING_TYPE, 0);
    fprintf(stderr, "Return: %d, %s\n", ret, strerror(errno));

    ret = mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string\"", MMV_STRING_TYPE, 0);
    fprintf(stderr, "Return: %d, %s\n", ret, strerror(errno));

    ret = mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string\"", MMV_STRING_TYPE, 0);
    fprintf(stderr, "Return: %d, %s\n", ret, strerror(errno));

    ret = mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string\"", MMV_STRING_TYPE, 0);
    fprintf(stderr, "Return: %d, %s\n", ret, strerror(errno));

    ret = mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string\"", MMV_STRING_TYPE, 0);
    fprintf(stderr, "Return: %d, %s\n", ret, strerror(errno));*/

    map = mmv_stats_start(registry);
    if (!map) {
	fprintf(stderr, "mmv_stats_start: %s - %s\n", file, strerror(errno));
	return 1;
    }

    mmv_stats_free(registry);
    return 0;
}
