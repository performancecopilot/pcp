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

/* construct the PMID for acct.control.enable_acct */
#define ACCT_CONTROL_ENABLE	pmID_build(3, 70, 28)

static int
get_current_enable_acct(void)
{
	pmID		pmid = ACCT_CONTROL_ENABLE;
	pmResult	*result;
	int		sts;

	sts = pmFetch(1, &pmid, &result);
	if (sts < 0)
	{
		if (pmDebugOptions.appl0)
			fprintf(stderr, "%s: %s pmFetch failed: %s\n",
				pmGetProgname(), "acctsw", pmErrStr(sts));
		return -1;
	}
	sts = result->vset[0]->numval == 1 ? result->vset[0]->vlist[0].value.lval : -1;
	pmFreeResult(result);
	return sts;
}

/* set acct.control.enable_acct to unsigned integer value */
static int
acctsw(unsigned int enable)
{
	pmResult	*result;
	pmValueSet	*vset;
	int		sts, prev_val;

	prev_val = get_current_enable_acct();
	if (prev_val < 0)
		return 1;

	result = calloc(1, sizeof(pmResult));
	vset = calloc(1, sizeof(pmValueSet));

	ptrverify(vset, "Malloc vset failed for %s enabling\n", "acct");
	ptrverify(result, "Malloc result failed for %s enabling\n", "acct");

	vset->vlist[0].inst = PM_IN_NULL;
	vset->vlist[0].value.lval = enable ? (prev_val + 1) : (prev_val > 0 ? (prev_val - 1) : 0);
	vset->valfmt = PM_VAL_INSITU;
	vset->numval = 1;
	vset->pmid = ACCT_CONTROL_ENABLE;

	result->vset[0] = vset;
	result->numpmid = 1;

	sts = pmStore(result);
	if (sts < 0 && pmDebugOptions.appl0)
		fprintf(stderr, "%s: %s pmStore failed: %s\n",
				pmGetProgname(), "acctsw", pmErrStr(sts));
	pmFreeResult(result);
	return sts < 0;
}

int
acctswon(void)
{
	return acctsw(1);
}

void
acctswoff(void)
{
	acctsw(0);
}

int
acctphotoproc(struct tstat **accproc, unsigned int *taskslen, struct timeval *current, struct timeval *delta)
{
	static int	setup;
	static pmID	pmids[ACCT_NMETRICS];
	static pmDesc	descs[ACCT_NMETRICS];
	pmResult	*result;
	char		**insts;
	int		*pids;
	unsigned long	count, i;
	double		curr_time, prev_time;
	int		pid, nrexit = 0;

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
	count = fetch_instances("acct", ACCT_GEN_ETIME, descs, &pids, &insts);
	if (count > *taskslen)
	{
		size_t	size = count * sizeof(struct tstat);

		*accproc = (struct tstat *)realloc(*accproc, size);
		ptrverify(*accproc, "Malloc failed for %lu exited processes\n", count);
		*taskslen = count;
	}

	supportflags |= ACCTACTIVE;
	curr_time = pmtimevalToReal(current);
	prev_time = pmtimevalSub(current, delta);

	for (i=0; i < count; i++)
	{
		/*
		** fill process info from accounting-record
		*/
		pid = pids[i];
		time_t acct_btime = extract_count_t_inst(result, descs, ACCT_GEN_BTIME, pid, i);
		float  acct_etime = extract_float_inst(result, descs, ACCT_GEN_ETIME, pid, i);
		double pexit_time = (double)acct_btime + acct_etime;
		if (pexit_time <= prev_time || curr_time < pexit_time)
			continue;

		struct tstat	*api;
		api = &(*accproc)[nrexit++];
		api->gen.state  = 'E';
		api->gen.pid    = pid;
		api->gen.tgid   = pid;
		api->gen.ppid   = extract_integer_inst(result, descs, ACCT_GEN_PPID, pid, i);
		api->gen.nthr   = 1;
		api->gen.isproc = 1;
		api->gen.excode = extract_integer_inst(result, descs, ACCT_GEN_EXCODE, pid, i);
		api->gen.ruid   = extract_integer_inst(result, descs, ACCT_GEN_UID, pid, i);
		api->gen.rgid   = extract_integer_inst(result, descs, ACCT_GEN_GID, pid, i);
		api->gen.btime  = (acct_btime - system_boottime) * 1000;
		api->gen.elaps  = acct_etime;
		api->cpu.stime  = extract_float_inst(result, descs, ACCT_CPU_STIME, pid, i) * 1000;
		api->cpu.utime  = extract_float_inst(result, descs, ACCT_CPU_UTIME, pid, i) * 1000;
		api->mem.minflt = extract_count_t_inst(result, descs, ACCT_MEM_MINFLT, pid, i);
		api->mem.majflt = extract_count_t_inst(result, descs, ACCT_MEM_MAJFLT, pid, i);
		api->dsk.rio    = extract_count_t_inst(result, descs, ACCT_DSK_RIO, pid, i);

		pmstrncpy(api->gen.name, sizeof(api->gen.name), insts[i]);
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
do_pacctdir(char *tagname, char *tagvalue)
{
	fprintf(stderr, "Warning: option '%s' specified (%s), "
			"but always ignored by %s\n",
			tagname, tagvalue, pmGetProgname());
	sleep(2);
}
