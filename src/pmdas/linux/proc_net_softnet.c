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
    int count;
    unsigned flags = 0;
    int cpu;
    FILE *fp;
    uint64_t filler;
    proc_net_softnet_t *sn = proc_net_softnet;

    /* size > (11*7)+1 bytes, where 7 == strlen("%08llx "), and +1 for '\0' */
    static char fmt[128] = { '\0' };

    if (fmt[0] == '\0') {
	int i;
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

	/* one-trip - allocate per-cpu counter arrays */
	memset(sn, 0, sizeof(proc_net_softnet_t));
	sn->processed = (uint64_t *)malloc(_pm_ncpus * sizeof(uint64_t));
	sn->dropped = (uint64_t *)malloc(_pm_ncpus * sizeof(uint64_t));
	sn->time_squeeze = (uint64_t *)malloc(_pm_ncpus * sizeof(uint64_t));
	sn->cpu_collision = (uint64_t *)malloc(_pm_ncpus * sizeof(uint64_t));
	sn->received_rps = (uint64_t *)malloc(_pm_ncpus * sizeof(uint64_t));
	sn->flow_limit_count = (uint64_t *)malloc(_pm_ncpus * sizeof(uint64_t));

	if (!sn->processed || !sn->dropped || !sn->time_squeeze ||
	    !sn->cpu_collision || !sn->received_rps || !sn->flow_limit_count) {
	    return -ENOMEM;
	}
    }

    if ((fp = linux_statsfile("/proc/net/softnet_stat", buf, sizeof(buf))) == NULL)
	return -oserror();
    for (cpu=0; cpu < _pm_ncpus; cpu++) {
	if (fgets(buf, sizeof(buf), fp) == NULL)
	    break;
	count = sscanf(buf, fmt,
		&sn->processed[cpu], &sn->dropped[cpu], &sn->time_squeeze[cpu],
		&filler, &filler, &filler, &filler, &filler,
		&sn->cpu_collision[cpu], &sn->received_rps[cpu], &sn->flow_limit_count[cpu]);
	sn->flags = 0;
	if (count >= 9)
	    sn->flags |= SN_PROCESSED | SN_DROPPED | SN_TIME_SQUEEZE | SN_CPU_COLLISION;
	if (count >= 10)
	    sn->flags |= SN_RECEIVED_RPS;
	if (count >= 11)
	    sn->flags |= SN_FLOW_LIMIT_COUNT;

	if (cpu > 0 && sn->flags != flags)
	    fprintf(stderr, "refresh_proc_net_softnet: warning: inconsistent flags, cpu %d\n", cpu);
	flags = sn->flags;
    }

    fclose(fp);
    return 0;
}
