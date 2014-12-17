#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "pmapi.h"

#include "config.h"

extern int conf_gen;

void
hotproc_init()
{
    char    h_configfile[MAXPATHLEN];
    FILE    *conf;
    int	    sep = __pmPathSeparator();


    snprintf(h_configfile, sizeof(h_configfile), "%s%c" "proc" "%c" "hotproc.conf",
	    pmGetConfig("PCP_PMDAS_DIR"), sep, sep);

    conf = open_config(h_configfile);

    /* Hotproc configured */
    if( conf != NULL ){
	if(read_config(conf)){
	    conf_gen = 1;
	}
	(void)fclose(conf);
    }
}
