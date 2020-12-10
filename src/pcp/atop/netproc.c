/*
** Copyright (C) 2020 Red Hat.
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2, or (at your option) any
** later version.
**
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU General Public License for more details.
*/

#include <pcp/pmapi.h>
#include "atop.h"
#include "photoproc.h"
#include "netprocmetrics.h"

void
netproc_probe(void)
{
	pmID	pmids[TASK_NET_NMETRICS];

	int sts = pmLookupName(TASK_NET_NMETRICS, netprocmetrics, pmids);
	if (sts == TASK_NET_NMETRICS)
		supportflags |= NETATOP;
	else
		supportflags &= ~NETATOP;
}

static void
netproc_update_task(struct tstat *task, int pid, pmResult *rp, pmDesc *dp, int offset)
{
	task->net.tcpsnd = extract_count_t_inst(rp, dp, TASK_NET_TCPSND, pid, offset);
	task->net.tcprcv = extract_count_t_inst(rp, dp, TASK_NET_TCPRCV, pid, offset);
	task->net.tcpssz = extract_count_t_inst(rp, dp, TASK_NET_TCPSSZ, pid, offset);
	task->net.tcprsz = extract_count_t_inst(rp, dp, TASK_NET_TCPRSZ, pid, offset);

	task->net.udpsnd = extract_count_t_inst(rp, dp, TASK_NET_UDPSND, pid, offset);
	task->net.udprcv = extract_count_t_inst(rp, dp, TASK_NET_UDPRCV, pid, offset);
	task->net.udpssz = extract_count_t_inst(rp, dp, TASK_NET_UDPSSZ, pid, offset);
	task->net.udprsz = extract_count_t_inst(rp, dp, TASK_NET_UDPRSZ, pid, offset);
}

void
netproc_update_tasks(struct tstat **tasks, unsigned long taskslen)
{
	static int 	setup;
	static pmID	pmids[TASK_NET_NMETRICS];
	static pmDesc	descs[TASK_NET_NMETRICS];
	pmResult	*result;
	char		**insts;
	int		*pids;
	int		netproc_insts_len, i, j;

	if (!setup)
	{
		setup_metrics(netprocmetrics, pmids, descs, TASK_NET_NMETRICS);
		setup = 1;
	}

	fetch_metrics("task_netproc", TASK_NET_NMETRICS, pmids, &result);
	netproc_insts_len = get_instances("task_netproc", TASK_NET_TCPSND, descs, &pids, &insts);

	for (i=0; i < netproc_insts_len; i++)
	{
		if (pmDebugOptions.appl0)
			fprintf(stderr, "%s: updating net info of process %d: %s\n",
				pmGetProgname(), pids[i], insts[i]);

		// for each instance of the netproc metrics, we need to find the index inside the tasks array
		// TODO: better algorithm to avoid O(m*n) complexity?
		for (j=0; j < taskslen; j++)
		{
			if ((*tasks)[j].gen.pid == pids[i])
				netproc_update_task(&(*tasks)[j], pids[i], result, descs, i);
		}
	}

	pmFreeResult(result);
	if (netproc_insts_len > 0) {
	    free(insts);
	    free(pids);
	}
}
