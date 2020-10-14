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
#include "atop.h"
#include "photoproc.h"
#include "acctmetrics.h"

/*
** Stub functions, disabling functionality that we're not supporting from
** the original atop (not necessarily because there's anything wrong with
** it, more because it needs to be re-thought in a distributed PCP world.
**
** e.g. netatopd and acctproc code would need to exist in PCP PMDAs and be
** accesses via PMAPI metric interfaces.
*/

void
netatop_ipopen(void)
{
}

void
netatop_probe(void)
{
}

void
netatop_signoff(void)
{
}

void
netatop_gettask(pid_t pid, char c, struct tstat *tstat)
{
	(void)pid;
	(void)c;
	(void)tstat;
}

unsigned int
netatop_exitstore(void)
{
	return 0;
}

void
netatop_exiterase(void)
{
}

void
netatop_exithash(char p)
{
	(void)p;
}

void
netatop_exitfind(unsigned long x, struct tstat *a, struct tstat *b)
{
	(void)x;
	(void)a;
	(void)b;
}

int
acctswon(void)
{
	return 0;
}

void
acctswoff(void)
{
}

unsigned long 
acctprocnt(void)
{
	return 0;
}

int
acctphotoproc(struct tstat **accproc, unsigned int *taskslen, double cur_time, double prev_time)
{
	static int setup;
	static pmID	pmids[ACCT_NMETRICS];
	static pmDesc	descs[ACCT_NMETRICS];
	pmResult	*result;
	char		**insts;
	int		*pids;
	unsigned long	count, i;

	int nrexit = 0;
	struct tstat *api;
	int pid;

	if (!setup)
	{
		setup = 1;
		setup_metrics(acctmetrics, pmids, descs, ACCT_NMETRICS);
		for (i = 0; i < ACCT_NMETRICS; i++)
		{
			if (descs[i].pmid == PM_ID_NULL)
				return 0;
		}
	}
	else if (!(supportflags & ACCTACTIVE))
		return 0;

	fetch_metrics("acct", ACCT_NMETRICS, pmids, &result);
	count = get_instances("acct", ACCT_GEN_ETIME, descs, &pids, &insts);
	if (count > *taskslen)
	{
		size_t	size = count * sizeof(struct tstat);

		*accproc = (struct tstat *)realloc(*accproc, size);
		ptrverify(*accproc, "Malloc failed for %lu exited processes\n", count);
		*taskslen = count;
	}

	supportflags |= ACCTACTIVE;

	for  (i=0; i < count; i++)
	{
		/*
		** fill process info from accounting-record
		*/
		pid = pids[i];
		time_t acct_btime = extract_count_t_inst(result, descs, ACCT_GEN_BTIME, pid);
		float  acct_etime = extract_float_inst(result, descs, ACCT_GEN_ETIME, pid);
		double pexit_time = (double)acct_btime + acct_etime;
		if (pexit_time <= prev_time || cur_time < pexit_time)
			continue;

		api = &(*accproc)[nrexit++];
		api->gen.state  = 'E';
		api->gen.pid    = pid;
		api->gen.tgid   = pid;
		api->gen.ppid   = extract_integer_inst(result, descs, ACCT_GEN_PPID, pid);
		api->gen.nthr   = 1;
		api->gen.isproc = 1;
		api->gen.excode = extract_integer_inst(result, descs, ACCT_GEN_EXCODE, pid);
		api->gen.ruid   = extract_integer_inst(result, descs, ACCT_GEN_UID, pid);
		api->gen.rgid   = extract_integer_inst(result, descs, ACCT_GEN_GID, pid);
		api->gen.btime  = (acct_btime - system_boottime) * 1000;
		api->gen.elaps  = acct_etime;
		api->cpu.stime  = extract_float_inst(result, descs, ACCT_CPU_STIME, pid) * 1000;
		api->cpu.utime  = extract_float_inst(result, descs, ACCT_CPU_UTIME, pid) * 1000;
		api->mem.minflt = extract_count_t_inst(result, descs, ACCT_MEM_MINFLT, pid);
		api->mem.majflt = extract_count_t_inst(result, descs, ACCT_MEM_MAJFLT, pid);
		api->dsk.rio    = extract_count_t_inst(result, descs, ACCT_DSK_RIO, pid);

		strcpy(api->gen.name, insts[i]);
	}

	pmFreeResult(result);
	if (count > 0)
	{
		free(insts);
		free(pids);
	}

	return nrexit;
}

void
acctrepos(unsigned int a)
{
	(void)a;
}

void
do_pacctdir(char *tagname, char *tagvalue)
{
	(void)tagname;
	(void)tagvalue;
}
