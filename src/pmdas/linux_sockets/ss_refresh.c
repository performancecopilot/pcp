/*
 * Copyright (c) 2021 Red Hat.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

#include "ss_stats.h"

static char *
ss_instname(ss_stats_t *ss, char *buf, int buflen)
{
    /* af/src:port */
    pmsprintf(buf, buflen, "%s%s%s", ss->af, ss->v6only ? "6/" : "/", ss->src);

    return buf;
}

static void
ss_free(void *p)
{
    ss_stats_t *ss = (ss_stats_t *)p;
    free(ss);
}

int
ss_refresh(int indom)
{
    FILE *fp;
    int sts = 0;
    ss_stats_t *ss, parsed_ss;
    int inst;
    char instname[128];
    char line[4096] = {0};

    if ((fp = ss_open_stream()) == NULL)
    	return -errno;

    pmdaCacheOp(indom, PMDA_CACHE_LOAD);
    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
    
    while (fgets(line, sizeof(line), fp) != NULL) {
	ss_parse(line, &parsed_ss);
	ss_instname(&parsed_ss, instname, sizeof(instname));
	ss = NULL;
	sts = pmdaCacheLookupName(indom, instname, &inst, (void **)&ss);
	if (sts < 0 || ss == NULL) {
	    /* new entry */
	    ss = (ss_stats_t *)malloc(sizeof(ss_stats_t));
	}
	*ss = parsed_ss;
	ss->instid = pmdaCacheStore(indom, PMDA_CACHE_ADD, instname, (void **)ss);
    }
    ss_close_stream(fp);

    /* purge inactive/closed sockets after 10min */
    pmdaCachePurgeCallback(indom, 600, ss_free);
    pmdaCacheOp(indom, PMDA_CACHE_SYNC); 

    return sts;
};
