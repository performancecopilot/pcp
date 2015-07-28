/*
 * Copyright (c) 2015 Red Hat.
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

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include "proc_net_softnet.h"

int
refresh_proc_net_softnet(proc_net_softnet_t *proc_net_softnet)
{
    char buf[1024];
    FILE *fp;
    uint64_t filler;
    proc_net_softnet_t sn;

    if ((fp = linux_statsfile("/proc/net/softnet_stat", buf, sizeof(buf))) == NULL)
	return -oserror();
    memset(proc_net_softnet, 0, sizeof(proc_net_softnet_t));
    while (fgets(buf, sizeof(buf), fp) != NULL) {
    /* summed, one line per-cpu */
	memset(&sn, 0, sizeof(sn));
	sscanf(buf, "%08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx",
	    &sn.processed, &sn.dropped, &sn.time_squeeze,
	    &filler, &filler, &filler, &filler, &filler,
	    &sn.cpu_collision, &sn.received_rps, &sn.flow_limit_count);
	proc_net_softnet->processed += sn.processed;
	proc_net_softnet->dropped += sn.dropped;
	proc_net_softnet->time_squeeze += sn.time_squeeze;
	proc_net_softnet->cpu_collision += sn.cpu_collision;
	proc_net_softnet->received_rps += sn.received_rps;
	proc_net_softnet->flow_limit_count += sn.flow_limit_count;
    }

    fclose(fp);
    return 0;
}
