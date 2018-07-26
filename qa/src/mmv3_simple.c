/* C language writer - using the application-level API, MMV v3 */
/* Build via: cc -g -Wall -lpcp_mmv -o mmv3_simple mm3v_simple.c */

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
        .shorttext = "2sh",
        .helptext = "2h",
    },
};

static mmv_indom2_t indoms[] = {
    {	.serial = 1,
	    .count = 2,
	    .instances = NULL,
	    .shorttext = "We can be heroes",
	    .helptext = "We can be heroes, just for one day",
    },
    {	.serial = 2,
	    .count = 3,
	    .instances = NULL,
	/* exercise no-help-text case */
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

    /*mmv_stats_add_metric(addr,metrics[1].name,metrics[1].item,metrics[1].type,
                         metrics[1].semantics,metrics[1].dimension,0,metrics[1].shorttext,
                         metrics[1].helptext);

    fprintf(stderr, "[CHECKING AFTER ADDING METRIC] \n");
    fprintf(stderr, "nindoms: %d \n", addr->nindoms);
    fprintf(stderr, "nmetrics: %d \n", addr->nmetrics);
    fprintf(stderr, "ninstances: %d \n", addr->ninstances);
    fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    fprintf(stderr, "version: %d \n", addr->version);
    
    _metric = addr->metrics;
    fprintf(stderr, "[CHECKING METRIC] \n");
    fprintf(stderr, "name: %s \n", _metric[1].name);
    fprintf(stderr, "shorthelp: %s \n", _metric[1].shorttext);
    fprintf(stderr, "longhelp: %s \n", _metric[1].helptext);*/


    /*mmv_stats_add_indom(addr,indoms[0].serial,indoms[0].shorttext,indoms[0].helptext);
    fprintf(stderr, "[CHECKING AFTER ADDING INDOM] \n");
    fprintf(stderr, "nindoms: %d \n", addr->nindoms);
    fprintf(stderr, "nmetrics: %d \n", addr->nmetrics);
    fprintf(stderr, "ninstances: %d \n", addr->ninstances);
    fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    fprintf(stderr, "version: %d \n", addr->version);*/
    
    //mmv_indom2_t * _indom = addr->indoms;
    //fprintf(stderr, "[CHECKING METRIC] \n");
    //fprintf(stderr, "serial: %d \n", _indom[0].serial);
    //fprintf(stderr, "shorthelp: %s \n", _indom[0].shorttext);
    //fprintf(stderr, "longhelp: %s \n", _indom[0].helptext);

    /*mmv_stats_add_indom(addr,indoms[1].serial,indoms[1].shorttext,indoms[1].helptext);
    fprintf(stderr, "[CHECKING AFTER ADDING INDOM] \n");
    fprintf(stderr, "nindoms: %d \n", addr->nindoms);
    fprintf(stderr, "nmetrics: %d \n", addr->nmetrics);
    fprintf(stderr, "ninstances: %d \n", addr->ninstances);
    fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    fprintf(stderr, "version: %d \n", addr->version);
    
    _indom = addr->indoms;
    fprintf(stderr, "[CHECKING METRIC] \n");
    fprintf(stderr, "serial: %d \n", _indom[1].serial);
    fprintf(stderr, "shorthelp: %s \n", _indom[1].shorttext);
    fprintf(stderr, "longhelp: %s \n", _indom[1].helptext);
    */
    // ADD LABELS
    char *auxn = "name";
    char *auxv = "value";
    char aux[MMV_LABELMAX];
    
    pmsprintf(aux, sizeof(aux), "{\"%s\":\"%s\"}", auxn, auxv);
    fprintf(stderr, "[CHECKING] \n");
    fprintf(stderr, "%s \n", aux);
    mmv_stats_add_registry_label(addr,auxn,auxv,MMV_STRING_TYPE);
    fprintf(stderr, "[CHECKING AFTER ADDING LABEL] \n");
    fprintf(stderr, "nindoms: %d \n", addr->nindoms);
    fprintf(stderr, "nmetrics: %d \n", addr->nmetrics);
    fprintf(stderr, "ninstances: %d \n", addr->ninstances);
    fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    fprintf(stderr, "version: %d \n", addr->version);

    mmv_label_t *lb = addr->labels;
    fprintf(stderr, "[CHECKING LABEL] \n");
    fprintf(stderr, "identity: %d \n", lb[0].identity);
    fprintf(stderr, "payload: %s \n", lb[0].payload);
    //printf("payload: %c \n", lb[0].payload[0]);
    //printf("payload: %c \n", lb[0].payload[1]);
    //printf("payload: %c \n", lb[0].payload[2]);
    //printf("payload: %c \n", lb[0].payload[3]);
    //printf("payload: %c \n", lb[0].payload[4]);
    //printf("payload: %c \n", lb[0].payload[5]);

    
    //char *auxtot = addr->labels[0].payload; 
    //strncpy(auxtot, auxn, strlen(auxn));
    //fprintf(stderr, "stringtot: %s \n", auxtot);
    //strncpy(&(addr->labels[0].payload[strlen(auxn)]), auxv, strlen(auxv));
    //auxtot[29] = '\0';
    //fprintf(stderr, "stringtot: %s \n", addr->labels[0].payload);
    
    //fprintf(stderr, "longhelp: %s \n", lb[0].helptext);



    

    
    //fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    //fprintf(stderr, "version: %d \n", addr->version); 


    mmv_stats_start(file,addr);

    value = mmv_lookup_value_desc(addr->addr, metrics[0].name, NULL);
    mmv_inc_value(addr->addr, value, 42);
    mmv_stats_free(file, addr);
    //fprintf(stderr, "File: %s \n", file); 

    return 0;
}
