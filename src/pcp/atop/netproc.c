/*
** Copyright (C) 2020,2024 Red Hat.
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
#include <pcp/libpcp.h>
#include "atop.h"
#include "photoproc.h"
#include "netprocmetrics.h"

void
netproc_probe(void)
{
	pmID	pmids[TASK_NET_NMETRICS];
	pmID	pmidsbpf[TASK_NETBPF_NMETRICS];

	int sts = pmLookupName(TASK_NET_NMETRICS, netprocmetrics, pmids);
	if (sts == TASK_NET_NMETRICS)
		supportflags |= NETATOP;
	else
		supportflags &= ~NETATOP;

	sts = pmLookupName(TASK_NETBPF_NMETRICS, netbpfprocmetrics, pmidsbpf);
	if (sts == TASK_NETBPF_NMETRICS)
		supportflags |= NETATOPBPF;
	else
		supportflags &= ~NETATOPBPF;
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

static void
netbpfproc_update_task(struct tstat *task, int pid, pmResult *rp, pmDesc *dp, int offset)
{
       task->net.tcpsnd = extract_count_t_inst(rp, dp, TASK_NETBPF_TCPSND, pid, offset);
       task->net.tcprcv = extract_count_t_inst(rp, dp, TASK_NETBPF_TCPRCV, pid, offset);
       task->net.tcpssz = extract_count_t_inst(rp, dp, TASK_NETBPF_TCPSSZ, pid, offset);
       task->net.tcprsz = extract_count_t_inst(rp, dp, TASK_NETBPF_TCPRSZ, pid, offset);

       task->net.udpsnd = extract_count_t_inst(rp, dp, TASK_NETBPF_UDPSND, pid, offset);
       task->net.udprcv = extract_count_t_inst(rp, dp, TASK_NETBPF_UDPRCV, pid, offset);
       task->net.udpssz = extract_count_t_inst(rp, dp, TASK_NETBPF_UDPSSZ, pid, offset);
       task->net.udprsz = extract_count_t_inst(rp, dp, TASK_NETBPF_UDPRSZ, pid, offset);
}

static __pmHashWalkState
tasks_ctl_destroy_callback(const __pmHashNode *tp, void *cp)
{
	(void)tp;
	(void)cp;
	return PM_HASH_WALK_DELETE_NEXT;
}

void
netproc_update_tasks(struct tstat **tasks, unsigned long taskslen)
{
	static int 	setup;
	static pmID	pmids[TASK_NET_NMETRICS];
	static pmDesc	descs[TASK_NET_NMETRICS];
	pmResult	*result;
	char		**insts;
	int		*instids;
	int		netproc_insts_len;
	__pmHashCtl	tasks_ctl; // mapping from pid to struct tstat*
	__pmHashNode	*task_node;
	struct tstat	*task;
	int		i, sts;

	if (!setup)
	{
		setup_metrics(netprocmetrics, pmids, descs, TASK_NET_NMETRICS);
		setup = 1;
	}

	fetch_metrics("task_netproc", TASK_NET_NMETRICS, pmids, &result);
	netproc_insts_len = get_instances("task_netproc", TASK_NET_TCPSND, descs, &instids, &insts);

	__pmHashInit(&tasks_ctl);
	sts = __pmHashPreAlloc(taskslen, &tasks_ctl);
	if (sts != 0)
	{
		fprintf(stderr, "%s: __pmHashPreAlloc failed: %s\n",
			pmGetProgname(), pmErrStr(sts));
		cleanstop(1);
	}

	for (i=0; i < taskslen; i++)
	{
		sts = __pmHashAdd((*tasks)[i].gen.pid, &(*tasks)[i], &tasks_ctl);
		if (sts < 0)
		{
			fprintf(stderr, "%s: __pmHashAdd failed: %s\n",
				pmGetProgname(), pmErrStr(sts));
			cleanstop(1);
			return;
		}
	}

	for (i=0; i < netproc_insts_len; i++)
	{
		task_node = __pmHashSearch(instids[i], &tasks_ctl);
		if (task_node)
		{
			task = task_node->data;
			if (pmDebugOptions.appl0)
				fprintf(stderr, "%s: updating net info of process %d: %s\n",
					pmGetProgname(), task->gen.pid, task->gen.name);

			netproc_update_task(task, instids[i], result, descs, i);
		}
	}

	__pmHashWalkCB(tasks_ctl_destroy_callback, NULL, &tasks_ctl);
	__pmHashClear(&tasks_ctl);
	pmFreeResult(result);
	if (netproc_insts_len > 0)
	{
	    free(insts);
	    free(instids);
	}
}

void
netbpfproc_update_tasks(struct tstat **tasks, unsigned long taskslen)
{
	static int 	setup;
	static pmID	pmids[TASK_NETBPF_NMETRICS];
	static pmDesc	descs[TASK_NETBPF_NMETRICS];
	pmResult	*result;
	char		**insts;
	int		*instids;
	int		netbpfproc_insts_len;
	__pmHashCtl	tasks_ctl; // mapping from pid to struct tstat*
	__pmHashNode	*task_node;
	struct tstat	*task;
	int		i, sts;

	if (!setup)
	{
		setup_metrics(netbpfprocmetrics, pmids, descs, TASK_NETBPF_NMETRICS);
		setup = 1;
	}

	fetch_metrics("task_netbpfproc", TASK_NETBPF_NMETRICS, pmids, &result);
	netbpfproc_insts_len = get_instances("task_netbpfproc", TASK_NETBPF_TCPSND, descs, &instids, &insts);

	__pmHashInit(&tasks_ctl);
	sts = __pmHashPreAlloc(taskslen, &tasks_ctl);
	if (sts != 0)
	{
		fprintf(stderr, "%s: __pmHashPreAlloc failed: %s\n",
			pmGetProgname(), pmErrStr(sts));
		cleanstop(1);
	}

	for (i=0; i < taskslen; i++)
	{
		sts = __pmHashAdd((*tasks)[i].gen.pid, &(*tasks)[i], &tasks_ctl);
		if (sts < 0)
		{
			fprintf(stderr, "%s: __pmHashAdd failed: %s\n",
				pmGetProgname(), pmErrStr(sts));
			cleanstop(1);
			return;
		}
	}

	for (i=0; i < netbpfproc_insts_len; i++)
	{
		task_node = __pmHashSearch(instids[i], &tasks_ctl);
		if (task_node)
		{
			task = task_node->data;
			if (pmDebugOptions.appl0)
				fprintf(stderr, "%s: updating net info of process %d: %s\n",
					pmGetProgname(), task->gen.pid, task->gen.name);

			netbpfproc_update_task(task, instids[i], result, descs, i);
		}
	}

	__pmHashWalkCB(tasks_ctl_destroy_callback, NULL, &tasks_ctl);
	__pmHashClear(&tasks_ctl);
	pmFreeResult(result);
	if (netbpfproc_insts_len > 0)
	{
	    free(insts);
	    free(instids);
	}
}
