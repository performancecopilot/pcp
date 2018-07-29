/* C language writer - using the application-level API, MMV v2 */
/* Build via: cc -g -Wall -lpcp_mmv -o mmv_simple mmv_simple.c */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

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

static mmv_instances2_t dogs[] = {
    {  0, "Fido" },
    {  1, "Brutus" },
};

int 
main(int argc, char **argv)
{
    char *file = (argc > 1) ? argv[1] : "simple3";
    mmv_registry_t *addr = mmv_stats_registry(file, 321, 0);

    if (!addr) {
        fprintf(stderr, "mmv_metric_register: %s - %s\n", file, strerror(errno));
        return 1;
    }
    fprintf(stderr, "[CHECKING BEFORE ADDING INDOM] \n");
    fprintf(stderr, "nindoms: %d \n", addr->nindoms);
    fprintf(stderr, "nmetrics: %d \n", addr->nmetrics);
    fprintf(stderr, "ninstances: %d \n", addr->ninstances);
    fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    fprintf(stderr, "version: %d \n", addr->version);
    fprintf(stderr, "File: %s \n", addr->file);
    fprintf(stderr, "File: %d \n", addr->cluster);
    fprintf(stderr, "File: %d \n", addr->flags);

    mmv_stats_add_indom(addr,indoms[0].serial,indoms[0].shorttext,indoms[0].helptext);
    fprintf(stderr, "[CHECKING AFTER ADDING INDOM] \n");
    fprintf(stderr, "nindoms: %d \n", addr->nindoms);
    fprintf(stderr, "nmetrics: %d \n", addr->nmetrics);
    fprintf(stderr, "ninstances: %d \n", addr->ninstances);
    fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    fprintf(stderr, "version: %d \n", addr->version);
    
    mmv_indom2_t * _indom = addr->indoms;
    fprintf(stderr, "[CHECKING INDOM] \n");
    fprintf(stderr, "serial: %d \n", _indom[0].serial);
    fprintf(stderr, "shorthelp: %s \n", _indom[0].shorttext);
    fprintf(stderr, "longhelp: %s \n", _indom[0].helptext);

    mmv_stats_add_indom(addr,indoms[1].serial,indoms[1].shorttext,indoms[1].helptext);
    fprintf(stderr, "[CHECKING AFTER ADDING INDOM] \n");
    fprintf(stderr, "nindoms: %d \n", addr->nindoms);
    fprintf(stderr, "nmetrics: %d \n", addr->nmetrics);
    fprintf(stderr, "ninstances: %d \n", addr->ninstances);
    fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    fprintf(stderr, "version: %d \n", addr->version);
    
    _indom = addr->indoms;
    fprintf(stderr, "[CHECKING INDOM] \n");
    fprintf(stderr, "serial: %d \n", _indom[1].serial);
    fprintf(stderr, "shorthelp: %s \n", _indom[1].shorttext);
    fprintf(stderr, "longhelp: %s \n", _indom[1].helptext);

    mmv_stats_add_instance(addr, 1, dogs[0].internal, dogs[0].external);

    /*for(i = 0; i < addr->nindoms; i++) {
        if (addr->indoms[i].serial == 2) {
            mmv_instances2_t *inst = calloc
        }
    }*/
    fprintf(stderr, "[CHECKING AFTER ADDING INDOM] \n");
    fprintf(stderr, "nindoms: %d \n", addr->nindoms);
    fprintf(stderr, "nmetrics: %d \n", addr->nmetrics);
    fprintf(stderr, "ninstances: %d \n", addr->ninstances);
    fprintf(stderr, "nlabels: %d \n", addr->nlabels);
    fprintf(stderr, "version: %d \n", addr->version);

    return 0;
}
