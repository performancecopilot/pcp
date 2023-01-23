/*
** Copyright (C) 2015-2017,2019-2022 Red Hat.
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
#include <ctype.h>

#include "atop.h"
#include "photoproc.h"
#include "procmetrics.h"

static pmID	pmids[TASK_NMETRICS];
static pmDesc	descs[TASK_NMETRICS];

/*
** sampled proc values into task structure, for one process/thread
*/
static void
update_task(struct tstat *task, int pid, char *name, pmResult *rp, pmDesc *dp, int offset)
{
	int key;
	char buf[32];
	char cgname[CGRLEN+2];
	char *nametail = name;

	memset(task, 0, sizeof(struct tstat));

	strsep(&nametail, " ");	/* remove process identifier prefix; might fail */
	strncpy(task->gen.cmdline, nametail ? nametail : name, CMDLEN);
	task->gen.cmdline[CMDLEN] = '\0';
	task->gen.isproc = 1;		/* thread/process marker */

	/* accumulate Pss from smaps (optional, relatively expensive) */
	if (!calcpss)
	    task->mem.pmem = (unsigned long long)-1;
	else
	    task->mem.pmem = extract_ucount_t_inst(rp, dp, TASK_MEM_PMEM, pid, offset);

	/* determine wchan if wanted (optional, relatively expensive) */
	if (getwchan)
	    extract_string_inst(rp, dp, TASK_GEN_WCHAN, &task->cpu.wchan[0],
				sizeof task->cpu.wchan, pid, offset);

	/* /proc/pid/cgroup */
	extract_string_inst(rp, dp, TASK_GEN_CONTAINER, &task->gen.container[0],
			    sizeof task->gen.container, pid, offset);
        if (task->gen.container[0] != '\0')
		supportflags |= DOCKSTAT;

	cgname[0] = '\0';
	extract_string_inst(rp, dp, TASK_GEN_CGROUP, &cgname[0],
			    sizeof cgname, pid, offset);
	if (cgname[0] == ':')
	{
		strncpy(task->gen.cgpath, &cgname[1], sizeof task->gen.cgpath);
		task->gen.cgpath[sizeof task->gen.cgpath - 1] = '\0';
		supportflags |= CGROUPV2;
	}

	/* /proc/pid/stat */
	extract_string_inst(rp, dp, TASK_GEN_NAME, &task->gen.name[0],
				sizeof(task->gen.name), pid, offset);
	extract_string_inst(rp, dp, TASK_GEN_STATE, &task->gen.state,
				sizeof(task->gen.state), pid, offset);

	task->gen.pid = extract_integer_inst(rp, dp, TASK_GEN_PID, pid, offset);
	task->gen.ppid = extract_integer_inst(rp, dp, TASK_GEN_PPID, pid, offset);
	if (task->gen.ppid <= 0 && pid != 1)
		task->gen.ppid = 1;
	task->mem.minflt = extract_count_t_inst(rp, dp, TASK_MEM_MINFLT, pid, offset);
	task->mem.majflt = extract_count_t_inst(rp, dp, TASK_MEM_MAJFLT, pid, offset);
	task->cpu.utime = extract_count_t_inst(rp, dp, TASK_CPU_UTIME, pid, offset);
	task->cpu.stime = extract_count_t_inst(rp, dp, TASK_CPU_STIME, pid, offset);
	task->cpu.prio = extract_integer_inst(rp, dp, TASK_CPU_PRIO, pid, offset);
	task->cpu.nice = extract_integer_inst(rp, dp, TASK_CPU_NICE, pid, offset);
	task->gen.btime = extract_integer_inst(rp, dp, TASK_GEN_BTIME, pid, offset);
	task->mem.vmem = extract_count_t_inst(rp, dp, TASK_MEM_VMEM, pid, offset);
	task->mem.rmem = extract_count_t_inst(rp, dp, TASK_MEM_RMEM, pid, offset);
	task->cpu.curcpu = extract_integer_inst(rp, dp, TASK_CPU_CURCPU, pid, offset);
	task->cpu.rtprio = extract_integer_inst(rp, dp, TASK_CPU_RTPRIO, pid, offset);
	task->cpu.policy = extract_integer_inst(rp, dp, TASK_CPU_POLICY, pid, offset);
	task->cpu.rundelay = extract_integer_inst(rp, dp, TASK_CPU_RUNDELAY, pid, offset);
	task->cpu.blkdelay = extract_integer_inst(rp, dp, TASK_CPU_BLKDELAY, pid, offset);

	task->cpu.cgcpuweight = -2;			/* not available */
	task->cpu.cgcpumax = task->cpu.cgcpumaxr = -2;	/* not available */

	/* /proc/pid/status */
	task->gen.nthr = extract_integer_inst(rp, dp, TASK_GEN_NTHR, pid, offset);
	task->gen.tgid = extract_integer_inst(rp, dp, TASK_GEN_TGID, pid, offset);
	if (task->gen.tgid <= 0)
		task->gen.tgid = pid;
	task->gen.ctid = extract_integer_inst(rp, dp, TASK_GEN_ENVID, pid, offset);
	task->gen.vpid = extract_integer_inst(rp, dp, TASK_GEN_VPID, pid, offset);

	task->gen.ruid = extract_integer_inst(rp, dp, TASK_GEN_RUID, pid, offset);
	task->gen.euid = extract_integer_inst(rp, dp, TASK_GEN_EUID, pid, offset);
	task->gen.suid = extract_integer_inst(rp, dp, TASK_GEN_SUID, pid, offset);
	task->gen.fsuid = extract_integer_inst(rp, dp, TASK_GEN_FSUID, pid, offset);
	task->gen.rgid = extract_integer_inst(rp, dp, TASK_GEN_RGID, pid, offset);
	task->gen.egid = extract_integer_inst(rp, dp, TASK_GEN_EGID, pid, offset);
	task->gen.sgid = extract_integer_inst(rp, dp, TASK_GEN_SGID, pid, offset);
	task->gen.fsgid = extract_integer_inst(rp, dp, TASK_GEN_FSGID, pid, offset);

	task->mem.vdata = extract_count_t_inst(rp, dp, TASK_MEM_VDATA, pid, offset);
	task->mem.vstack = extract_count_t_inst(rp, dp, TASK_MEM_VSTACK, pid, offset);
	task->mem.vexec = extract_count_t_inst(rp, dp, TASK_MEM_VEXEC, pid, offset);
	task->mem.vlibs = extract_count_t_inst(rp, dp, TASK_MEM_VLIBS, pid, offset);
	task->mem.vswap = extract_count_t_inst(rp, dp, TASK_MEM_VSWAP, pid, offset);
	task->mem.vlock = extract_count_t_inst(rp, dp, TASK_MEM_VLOCK, pid, offset);

	task->mem.cgmemmax = task->mem.cgmemmaxr = -2;	/* not available */
	task->mem.cgswpmax = task->mem.cgswpmaxr = -2;	/* not available */

	/* /proc/pid/io */
	task->dsk.rsz = extract_count_t_inst(rp, dp, TASK_DSK_RSZ, pid, offset);
	task->dsk.wsz = extract_count_t_inst(rp, dp, TASK_DSK_WSZ, pid, offset);
	task->dsk.cwsz = extract_count_t_inst(rp, dp, TASK_DSK_CWSZ, pid, offset);

	/* user names (cached) */
	key = task->gen.ruid;
	if (get_username(task->gen.ruid) == NULL &&
	    extract_string_inst(rp, dp, TASK_GEN_RUIDNM, buf, sizeof buf, pid, offset))
		add_username(key, buf);
	if (key != task->gen.euid && get_username(task->gen.euid) == NULL &&
	    extract_string_inst(rp, dp, TASK_GEN_EUIDNM, buf, sizeof buf, pid, offset))
		add_username(key, buf);
	if (key != task->gen.suid && get_username(task->gen.suid) == NULL &&
	    extract_string_inst(rp, dp, TASK_GEN_SUIDNM, buf, sizeof buf, pid, offset))
		add_username(key, buf);
	if (key != task->gen.fsuid && get_username(task->gen.fsuid) == NULL &&
	    extract_string_inst(rp, dp, TASK_GEN_FSUIDNM, buf, sizeof buf, pid, offset))
		add_username(key, buf);

	/* group names (cached) */
	key = task->gen.rgid;
	if (get_groupname(task->gen.rgid) == NULL &&
	    extract_string_inst(rp, dp, TASK_GEN_RGIDNM, buf, sizeof buf, pid, offset))
		add_groupname(key, buf);
	if (key != task->gen.egid && get_groupname(task->gen.egid) == NULL &&
	    extract_string_inst(rp, dp, TASK_GEN_EGIDNM, buf, sizeof buf, pid, offset))
		add_groupname(key, buf);
	if (key != task->gen.sgid && get_groupname(task->gen.sgid) == NULL &&
	    extract_string_inst(rp, dp, TASK_GEN_SGIDNM, buf, sizeof buf, pid, offset))
		add_groupname(key, buf);
	if (key != task->gen.fsgid && get_groupname(task->gen.fsgid) == NULL &&
	    extract_string_inst(rp, dp, TASK_GEN_FSGIDNM, buf, sizeof buf, pid, offset))
		add_groupname(key, buf);

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
  	   case 'I':
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

/*
** kernel.all.pid_max is unavailable, fall back to heuristics.
** Firstly seek out the maximum PID from the proc indom for a
** single sample, or (in the case of an archive) all available
** samples.  If that fails fallback to some sensible default.
*/
int
getmaxpid(void)
{
	char		**insts;
	int		*pids, maxpid = 0;
	unsigned long	i, count;

	count = get_instances("maxpid", TASK_GEN_PID, descs, &pids, &insts);
	if (count > 0) {
		for (i = 0; i < count; i++)
			if (pids[i] > maxpid)
				maxpid = pids[i];
		free(insts);
		free(pids);
	}
	if (!maxpid)
		return (1 << 15);
	return maxpid;
}

void
setup_photoproc(void)
{
	unsigned long	i;

	if (!hotprocflag)
		for (i = 0; i < TASK_NMETRICS; i++)
			procmetrics[i] += 3;	/* skip "hot" */

	setup_metrics(procmetrics, pmids, descs, TASK_NMETRICS);

	/* check if per-process network metrics are available */
	netproc_probe();
}

unsigned long
photoproc(struct tstat **tasks, unsigned long *taskslen)
{
	static int	setup;
	static pmID	pssid;
	static pmID	wchanid;
	pmResult	*result;
	char		**insts;
	int		*pids, offset;
	unsigned long	count, i;

	if (!setup)
	{
		wchanid = pmids[TASK_GEN_WCHAN];
		pssid = pmids[TASK_MEM_PMEM];
		setup = 1;
	}

	/*
	** reading the smaps file for every process with every sample
	** is a really 'expensive' from a CPU consumption point-of-view,
	** so gathering this info is optional
	*/
	if (!calcpss)
		pmids[TASK_MEM_PMEM] = PM_ID_NULL;
	else
		pmids[TASK_MEM_PMEM] = pssid;
	/*
	** determine thread's wchan, if wanted ('expensive' from
	** a CPU consumption point-of-view)
	*/
	if (!getwchan)
		pmids[TASK_GEN_WCHAN] = PM_ID_NULL;
	else
		pmids[TASK_GEN_WCHAN] = wchanid;

	fetch_metrics("task", TASK_NMETRICS, pmids, &result);

	/* extract external process names (insts) */
	count = fetch_instances("task", TASK_GEN_NAME, descs, &pids, &insts);
	if (count > *taskslen)
	{
		size_t	size = count * sizeof(struct tstat);

		*tasks = (struct tstat *)realloc(*tasks, size);
		ptrverify(*tasks, "photoproc [%ld]\n", (long)size);
		*taskslen = count;
	}

	supportflags &= ~DOCKSTAT;

	for (i=0; i < count; i++)
	{
		if (pmDebugOptions.appl0)
			fprintf(stderr, "%s: updating process %d: %s\n",
				pmGetProgname(), pids[i], insts[i]);
		offset = get_instance_index(result, TASK_GEN_PID, pids[i]);
		update_task(&(*tasks)[i], pids[i], insts[i], result, descs, offset);
	}

	if (supportflags & NETATOP)
		netproc_update_tasks(tasks, count);

	if (pmDebugOptions.appl0)
		fprintf(stderr, "%s: done %lu processes\n", pmGetProgname(), count);

	pmFreeResult(result);
	if (count > 0) {
		free(insts);
		free(pids);
	}

	return count;
}
