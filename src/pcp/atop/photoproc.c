/*
** Copyright (C) 2015 Red Hat.
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
#include <pcp/impl.h>
#include <ctype.h>

#include "atop.h"
#include "photoproc.h"
#include "procmetrics.h"

/*
** sampled proc values into task structure, for one process/thread
*/
static void
update_task(struct tstat *task, int pid, char *name, pmResult *rp, pmDesc *dp)
{
	memset(task, 0, sizeof(struct tstat));

	strsep(&name, " ");	/* remove process identifier prefix */
	strncpy(task->gen.cmdline, name, CMDLEN);
	task->gen.cmdline[CMDLEN] = '\0';
	task->gen.isproc = 1;		/* thread/process marker */
	task->gen.nthr = 1;		/* for compat with 2.4 */

	/* accumulate Pss from smaps (optional, relatively expensive) */
	task->mem.pmem = (unsigned long long)-1LL;

	/* /proc/pid/stat */
	extract_string_inst(rp, dp, TASK_GEN_NAME, &task->gen.name[0],
				sizeof(task->gen.name), pid);
	extract_string_inst(rp, dp, TASK_GEN_STATE, &task->gen.state,
				sizeof(task->gen.state), pid);

	task->gen.pid = extract_integer_inst(rp, dp, TASK_GEN_PID, pid);
	task->gen.ppid = extract_integer_inst(rp, dp, TASK_GEN_PPID, pid);
	task->mem.minflt = extract_count_t_inst(rp, dp, TASK_MEM_MINFLT, pid);
	task->mem.majflt = extract_count_t_inst(rp, dp, TASK_MEM_MAJFLT, pid);
	task->cpu.utime = extract_count_t_inst(rp, dp, TASK_CPU_UTIME, pid);
	task->cpu.stime = extract_count_t_inst(rp, dp, TASK_CPU_STIME, pid);
	task->cpu.prio = extract_integer_inst(rp, dp, TASK_CPU_PRIO, pid);
	task->cpu.nice = extract_integer_inst(rp, dp, TASK_CPU_NICE, pid);
	task->gen.btime = extract_integer_inst(rp, dp, TASK_GEN_BTIME, pid);
	task->mem.vmem = extract_count_t_inst(rp, dp, TASK_MEM_VMEM, pid);
	task->mem.rmem = extract_count_t_inst(rp, dp, TASK_MEM_RMEM, pid);
	task->cpu.curcpu = extract_integer_inst(rp, dp, TASK_CPU_CURCPU, pid);
	task->cpu.rtprio = extract_integer_inst(rp, dp, TASK_CPU_RTPRIO, pid);
	task->cpu.policy = extract_integer_inst(rp, dp, TASK_CPU_POLICY, pid);

	/* /proc/pid/status */
	task->gen.nthr = extract_integer_inst(rp, dp, TASK_GEN_NTHR, pid);
	task->gen.tgid = extract_integer_inst(rp, dp, TASK_GEN_TGID, pid);
	task->gen.ctid = extract_integer_inst(rp, dp, TASK_GEN_ENVID, pid);
	task->gen.vpid = extract_integer_inst(rp, dp, TASK_GEN_VPID, pid);
	task->gen.ruid = extract_integer_inst(rp, dp, TASK_GEN_RUID, pid);
	task->gen.euid = extract_integer_inst(rp, dp, TASK_GEN_EUID, pid);
	task->gen.suid = extract_integer_inst(rp, dp, TASK_GEN_SUID, pid);
	task->gen.fsuid = extract_integer_inst(rp, dp, TASK_GEN_FSUID, pid);
	task->gen.rgid = extract_integer_inst(rp, dp, TASK_GEN_RGID, pid);
	task->gen.egid = extract_integer_inst(rp, dp, TASK_GEN_EGID, pid);
	task->gen.sgid = extract_integer_inst(rp, dp, TASK_GEN_SGID, pid);
	task->gen.fsgid = extract_integer_inst(rp, dp, TASK_GEN_FSGID, pid);
	task->mem.vdata = extract_count_t_inst(rp, dp, TASK_MEM_VDATA, pid);
	task->mem.vstack = extract_count_t_inst(rp, dp, TASK_MEM_VSTACK, pid);
	task->mem.vexec = extract_count_t_inst(rp, dp, TASK_MEM_VEXEC, pid);
	task->mem.vlibs = extract_count_t_inst(rp, dp, TASK_MEM_VLIBS, pid);
	task->mem.vswap = extract_count_t_inst(rp, dp, TASK_MEM_VSWAP, pid);

	/* /proc/pid/io */
	task->dsk.rsz = extract_count_t_inst(rp, dp, TASK_DSK_RSZ, pid);
	task->dsk.wsz = extract_count_t_inst(rp, dp, TASK_DSK_WSZ, pid);
	task->dsk.cwsz = extract_count_t_inst(rp, dp, TASK_DSK_CWSZ, pid);

	/*
 	** normalization
	*/
	task->cpu.prio   += 100; 	/* was subtracted by kernel */

	switch (task->gen.state)
	{
  	   case 'R':
		task->gen.nthrrun  = 1;
		break;
  	   case 'S':
		task->gen.nthrslpi = 1;
		break;
  	   case 'D':
		task->gen.nthrslpu = 1;
		break;
	}

	if (task->gen.tgid > 0 && task->gen.tgid != pid)
		task->gen.isproc = 0;

	if (task->dsk.rsz >= 0)
		supportflags |= IOSTAT;

	task->dsk.rio = task->dsk.rsz /= 512; /* sectors */
	task->dsk.wio = task->dsk.wsz /= 512; /* sectors */
	task->dsk.cwsz /= 512;		    /* sectors */
}

int
photoproc(struct tstat **tasks, int *taskslen)
{
	static int	setup;
	static pmID	pmids[TASK_NMETRICS];
	static pmDesc	descs[TASK_NMETRICS];
	pmResult	*result;
	char		**insts;
	int		*pids, count, i;

	if (!setup)
	{
		setup_metrics(procmetrics, pmids, descs, TASK_NMETRICS);
		setup = 1;
	}

	fetch_metrics("task", TASK_NMETRICS, pmids, &result);

	/* extract external process names (insts) */
	count = get_instances("task", TASK_GEN_NAME, descs, &pids, &insts);
	if (count > *taskslen)
	{
		size_t	size;
		int	ents = (*taskslen + PROCCHUNK);

		if (count > ents)
			ents = count;
		size = ents * sizeof(struct tstat);
		*tasks = (struct tstat *)realloc(*tasks, size);
		ptrverify(*tasks, "photoproc [%ld]\n", (long)size);

		*taskslen = ents;
	}

	for (i=0; i < count; i++)
	{
		if (pmDebug & DBG_TRACE_APPL0)
			fprintf(stderr, "%s: updating process %d: %s\n",
				pmProgname, pids[i], insts[i]);
		update_task(&(*tasks)[i], pids[i], insts[i], result, descs);
	}
	if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "%s: done %d processes\n", pmProgname, count);

	pmFreeResult(result);
	free(insts);
	free(pids);

	return count;
}
