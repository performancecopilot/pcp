/* C language writer - using the application-level API, MMV v2 */
/* Build via: cc -g -Wall -lpcp_mmv -o mmv_simple mmv_simple.c */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

static mmv_metric2_t metrics[] = {
    {   .name = "simple3.counter",
        .item = 1,
        .type = MMV_TYPE_U32,
        .semantics = MMV_SEM_COUNTER,   
        .dimension = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE),
        .shorttext = "shortext",
        .helptext = "helptext",
    },
    {   .name =
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
    char *file = (argc > 1) ? argv[1] : "simple3";
    mmv_registry_t *addr = mmv_stats_registry(file, 321, 0);

    if (!addr) {
        fprintf(stderr, "mmv_metric_register: %s - %s\n", file, strerror(errno));
        return 1;
    }
    fprintf(stderr, "[CHECKING BEFORE ADDING METRIC] \n");
    fprintf(stderr, "nindoms: %d \n", addr->nindoms);
    fprintf(stderr, "nmetrics: %d \n", addr->nmetrics);
    fprintf(stderr, "ninstances: %d \n", addr->ninstances);
    fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    fprintf(stderr, "version: %d \n", addr->version);
    fprintf(stderr, "File: %s \n", file);
    fprintf(stderr, "File: %s \n", addr->file);
    fprintf(stderr, "File: %d \n", addr->cluster);
    fprintf(stderr, "File: %d \n", addr->flags);
    fprintf(stderr, "File: %s \n", metrics[0].name);
    //char * aux = metrics[0].name;
    //fprintf(stderr, "File: %d \n", sizeof(metrics[0].name));
    //strncpy(aux, metrics[0].name, sizeof(metrics[0].name));
    //fprintf(stderr, "File: %s \n", aux);
    

    // Add metric
    mmv_stats_add_metric(addr,metrics[0].name,metrics[0].item,metrics[0].type,
                         metrics[0].semantics,metrics[0].dimension,0,metrics[0].shorttext,
                         metrics[0].helptext);
    fprintf(stderr, "[CHECKING AFTER ADDING METRIC] \n");
    fprintf(stderr, "nindoms: %d \n", addr->nindoms);
    fprintf(stderr, "nmetrics: %d \n", addr->nmetrics);
    fprintf(stderr, "ninstances: %d \n", addr->ninstances);
    fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    fprintf(stderr, "version: %d \n", addr->version);
    
    mmv_metric2_t * _metric = addr->metrics;
    fprintf(stderr, "[CHECKING METRIC] \n");
    fprintf(stderr, "name: %s \n", _metric[0].name);
    fprintf(stderr, "shorthelp: %s \n", _metric[0].shorttext);
    fprintf(stderr, "longhelp: %s \n", _metric[0].helptext);
    

    
    //fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    //fprintf(stderr, "version: %d \n", addr->version); 


    //void *addr_file = mmv_stats_start(file,addr);

    //value = mmv_lookup_value_desc(addr_file, metrics[0].name, NULL);
    //mmv_inc_value(addr_file, value, 42);
    //fprintf(stderr, "File: %s \n", file); 

    return 0;
}
