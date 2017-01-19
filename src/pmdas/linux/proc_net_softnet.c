/*
 * Copyright (c) 2015-2017 Red Hat.
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
#include "linux.h"
#include "proc_net_softnet.h"

int
refresh_proc_net_softnet(proc_net_softnet_t *all)
{
    int		i, cpu;
    char	buf[1024];
    pmInDom	cpus = INDOM(CPU_INDOM);
    percpu_t	*cp;
    softnet_t	*snp;
    uint64_t	filler;
    FILE	*fp;
    static int	logonce;

    /* size > (11*7)+1 bytes, where 7 == strlen("%08llx "), and +1 for '\0' */
    static char fmt[128] = { '\0' };

    if (fmt[0] == '\0') {
	/*
	 * one trip initialization to decide the correct sscanf format
	 * for a uint64_t data type .. needs to be
	 * "%08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx"
	 * or
	 * "%08llx %08llx %08llx %08llx %08llx %08llx %08llx %08llx %08llx %08llx %08llx"
	 */
	fmt[0] = '\0';
	if (strcmp(FMT_INT64, "lld") == 0) {
	    for (i = 0; i < 11; i++)
		strcat(fmt, "%08llx ");
	}
	else {
	    for (i = 0; i < 11; i++)
		strcat(fmt, "%08lx ");
	}
	/* chop off last ' ' */
	fmt[strlen(fmt)] = '\0';
    }

    memset(all, 0, sizeof(*all));
    if ((fp = linux_statsfile("/proc/net/softnet_stat", buf, sizeof(buf))) == NULL)
	return -oserror();

    /* walk over the online CPUs (only) - refreshed via /proc/stat already */
    for (pmdaCacheOp(cpus, PMDA_CACHE_WALK_REWIND);;) {
	if ((cpu = pmdaCacheOp(cpus, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (pmdaCacheLookup(cpus, cpu, NULL, (void **)&cp) != PMDA_CACHE_ACTIVE)
	    continue;
	if (!cp->softnet &&
	    (cp->softnet = malloc(sizeof(*cp->softnet))) == NULL) {
	    fprintf(stderr, "refresh_proc_net_softnet: out of memory, cpu %d\n", cpu);
	    break;
	}
	if (fgets(buf, sizeof(buf), fp) == NULL) {
	    fprintf(stderr, "refresh_proc_net_softnet: warning: insufficient data, cpu %d\n", cpu);
	    break;
	}
	snp = cp->softnet;
	memset(snp, 0, sizeof(*snp));
	i = sscanf(buf, fmt, &snp->processed, &snp->dropped, &snp->time_squeeze,
		&filler, &filler, &filler, &filler, &filler,
		&snp->cpu_collision, &snp->received_rps, &snp->flow_limit_count);

	/* update aggregate CPU stats as well */
	all->processed += snp->processed;
	all->dropped += snp->dropped;
	all->time_squeeze += snp->time_squeeze;
	all->cpu_collision += snp->cpu_collision;
	all->received_rps += snp->received_rps;
	all->flow_limit_count += snp->flow_limit_count;

	if (i >= 9)
	    snp->flags |= SN_PROCESSED | SN_DROPPED | SN_TIME_SQUEEZE | SN_CPU_COLLISION;
	if (i >= 10)
	    snp->flags |= SN_RECEIVED_RPS;
	if (i >= 11)
	    snp->flags |= SN_FLOW_LIMIT_COUNT;

	if (cpu > 0 && snp->flags != all->flags && logonce <= 1) {
	    fprintf(stderr, "refresh_proc_net_softnet: warning: inconsistent flags, cpu %d\n", cpu);
	    logonce = 1;
	}
	all->flags = snp->flags;
    }
    if (logonce)
	logonce++;	/* remember for next time, limit logging */

    fclose(fp);
    return 0;
}
