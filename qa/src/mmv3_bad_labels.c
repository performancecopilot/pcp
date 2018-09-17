/* C language checker - using the application-level API, MMV v3 */
/* Build via: cc -g -Wall -lpcp_mmv -o mmv3_bad_labels mm3_bad_labels.c */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

int
main(int argc, char **argv)
{
    int			sts;
    char		*file = (argc > 1) ? argv[1] : "bad_labels";
    mmv_registry_t	*registry = mmv_stats_registry(file, 321, 0);

    if (!registry) {
	fprintf(stderr, "mmv_stats_registry: %s - %s\n", file, strerror(errno));
	return 1;
    }

    sts = mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string1\"", MMV_STRING_TYPE, 0);
    if (sts < 0) perror("failed string - string1");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "string2\"", MMV_STRING_TYPE, 0);
    if (sts < 0) perror("failed string - string2");
    
    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"string3", MMV_STRING_TYPE, 0);
    if (sts < 0) perror("failed string - string3");
    
    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"stri\"n\"g4\"", MMV_STRING_TYPE, 0);
    if (sts < 0) perror("failed string - string4");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"stri\"", MMV_STRING_TYPE, 0);
    if (sts < 0) perror("failed string - stri");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"true\"", MMV_BOOLEAN_TYPE, 0);
    if (sts < 0) perror("failed boolean - \"true\"");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "True", MMV_BOOLEAN_TYPE, 0);
    if (sts < 0) perror("failed boolean - True");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "False", MMV_BOOLEAN_TYPE, 0);
    if (sts < 0) perror("failed boolean - False");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"false\"", MMV_BOOLEAN_TYPE, 0);
    if (sts < 0) perror("failed boolean - \"false\"");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "2.0", MMV_NUMBER_TYPE, 0);
    if (sts < 0) perror("failed number - 2.0");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "2.2", MMV_NUMBER_TYPE, 0);
    if (sts < 0) perror("failed number - 2.2");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "1", MMV_NUMBER_TYPE, 0);
    if (sts < 0) perror("failed number - 1");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "-12", MMV_NUMBER_TYPE, 0);
    if (sts < 0) perror("failed number - 12");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"12\"", MMV_NUMBER_TYPE, 0);
    if (sts < 0) perror("failed number - \"12\"");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "12.13.13", MMV_NUMBER_TYPE, 0);
    if (sts < 0) perror("failed number - 12.13.13");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "12.13.13", MMV_NULL_TYPE, 0);
    if (sts < 0) perror("failed null - 12.13.13");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "null", MMV_NULL_TYPE, 0);
    if (sts < 0) perror("failed null - null");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[1,2,3]", MMV_ARRAY_TYPE, 0);
    if (sts < 0) perror("failed array - [1,2,3]");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "1,2,3]", MMV_ARRAY_TYPE, 0);
    if (sts < 0) perror("failed array - 1,2,3]");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[1,2,3", MMV_ARRAY_TYPE, 0);
    if (sts < 0) perror("failed array - [1,2,3");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[1,2 3]", MMV_ARRAY_TYPE, 0);
    if (sts < 0) perror("failed array - [1,2 3");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "[\"123\",12]", MMV_ARRAY_TYPE, 0);
    if (sts < 0) perror("failed array - [\"123\",12]");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{\"a\":1,\"b\":2}", MMV_MAP_TYPE, 0);
    if (sts < 0) perror("failed map - {\"a\":1,\"b\":2}");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "\"a\":1,\"b\":2}", MMV_MAP_TYPE, 0);
    if (sts < 0) perror("failed map - \"a\":1,\"b\":2}");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{\"a\":1,\"b\":2", MMV_MAP_TYPE, 0);
    if (sts < 0) perror("failed map - {\"a\":1,\"b\":2");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{a\":1,\"b\":2}", MMV_MAP_TYPE, 0);
    if (sts < 0) perror("failed map - {a\":1,\"b\":2}");

    sts =  mmv_stats_add_registry_label(registry,
		    "registry_label", "{a:1,\"b\":2}", MMV_MAP_TYPE, 0);
    if (sts < 0) perror("failed map - {a:1,\"b\":2}");

    mmv_stats_free(registry);
    return 0;
}
