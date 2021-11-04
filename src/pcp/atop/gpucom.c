/*
** Copyright (C) 2018 Gerlof Langeveld <gerlof.langeveld@atoptool.nl>
** Copyright (C) 2019,2021 Red Hat.
**
** This source-file contains functions to interface with the nvidia
** agent, which maintains statistics about the processor and memory
** utilization of the GPUs.
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
#include "photosyst.h"
#include "photoproc.h"
#include "gpumetrics.h"
#include "gpucom.h"

static pmID     pmids[GPU_NMETRICS];
static pmDesc   descs[GPU_NMETRICS];

void
setup_gpuphotoproc(void)
{
        setup_metrics(gpumetrics, pmids, descs, GPU_NMETRICS);
}

static void
update_gpupid(struct gpu *g, int id, pmResult *rp, pmDesc *dp, int offset)
{
	memset(g, 0, sizeof(*g));
	if (extract_count_t_inst(rp, dp, GPU_PROC_RUNNING, id, offset) == 0)
		g->state = 'E';
	else
		g->state = 'A';
	g->gpubusy = extract_count_t_inst(rp, dp, GPU_PROC_GPUBUSY, id, offset);
	g->membusy = extract_count_t_inst(rp, dp, GPU_PROC_MEMBUSY, id, offset);
	g->timems = extract_count_t_inst(rp, dp, GPU_PROC_TIME, id, offset);
	g->memnow = extract_count_t_inst(rp, dp, GPU_PROC_MEMUSED, id, offset);
	g->memnow /= 1024;	/* convert to KiB */
	g->memcum = extract_count_t_inst(rp, dp, GPU_PROC_MEMACCUM, id, offset);
	g->memcum /= 1024;	/* convert to KiB */
	g->sample = extract_count_t_inst(rp, dp, GPU_PROC_SAMPLES, id, offset);
	g->nrgpus = extract_count_t_inst(rp, dp, GPU_PROC_NGPUS, id, offset);
	g->gpulist = extract_integer_inst(rp, dp, GPU_PROC_GPULIST, id, offset);
}

unsigned long
gpuphotoproc(struct gpupidstat **gpp, unsigned int *gpplen)
{
	pmResult        	*result;
	char			**insts;
	int			*pids;
	unsigned long		count, i;
	struct gpupidstat	*gp;

	fetch_metrics("gpu", GPU_NMETRICS, pmids, &result);

	/* extract external process names (insts) */
	count = fetch_instances("gpu", GPU_PROC_GPUBUSY, descs, &pids, &insts);
	if (count > *gpplen)
	{
		size_t  size = count * sizeof(struct gpupidstat);

		*gpp = (struct gpupidstat *)realloc(*gpp, size);
		ptrverify(*gpp, "gpuphotoproc [%ld]\n", (long)size);
		*gpplen = count;
	}
	gp = *gpp;

	for (i=0; i < count; i++)
	{
		if (pmDebugOptions.appl0)
			fprintf(stderr, "%s: updating process %d: %s\n",
				pmGetProgname(), pids[i], insts[i]);
		gp[i].pid = pids[i];
		update_gpupid(&gp[i].gpu, pids[i], result, descs, i);
	}

	if (pmDebugOptions.appl0)
		fprintf(stderr, "%s: %lu GPU processes\n", pmGetProgname(), count);

	pmFreeResult(result);
	if (count > 0) {
		free(insts);
		free(pids);
	}

	return count;
}

static int
compgpupid(const void *a, const void *b)
{
	return (*((struct gpupidstat **)a))->pid - (*((struct gpupidstat **)b))->pid;
}

/*
** Merge the GPU per-process counters with the other
** per-process counters
*/

void
gpumergeproc(struct tstat      *curtpres, int ntaskpres,
             struct tstat      *curpexit, int nprocexit,
             struct gpupidstat *gpuproc,  int nrgpuproc)
{
	struct gpupidstat	**gpp;
	int 			t, g, gpuleft = nrgpuproc;

	/*
 	** make pointer list for elements in gpuproc
	*/
	gpp = calloc(nrgpuproc, sizeof(struct gpupidstat *));
	ptrverify(gpp, "Malloc failed for process list\n");

	for (g=0; g < nrgpuproc; g++)
		gpp[g] = gpuproc + g;

	/*
   	** sort the list with pointers in order of pid
	*/
	if (nrgpuproc > 1)
        	qsort(gpp, nrgpuproc, sizeof(struct gpupidstat *), compgpupid);

	/*
	** accumulate entries that contain stats from same PID
	** on different GPUs
	*/
	for (g=1; g < nrgpuproc; g++)
	{
		if (gpp[g-1]->pid == gpp[g]->pid)
		{
			struct gpupidstat *p = gpp[g-1], *q = gpp[g];

			p->gpu.nrgpus  += q->gpu.nrgpus;
			p->gpu.gpulist |= q->gpu.gpulist;

			if (p->gpu.gpubusy != -1)
				p->gpu.gpubusy += q->gpu.gpubusy;

			if (p->gpu.membusy != -1)
				p->gpu.membusy += q->gpu.membusy;

			if (p->gpu.timems != -1)
				p->gpu.timems += q->gpu.timems;

			p->gpu.memnow += q->gpu.memnow;
			p->gpu.memcum += q->gpu.memcum;
			p->gpu.sample += q->gpu.sample;

			if (nrgpuproc-g-1 > 0)
				memmove(&(gpp[g]), &(gpp[g+1]),
					(nrgpuproc-g-1) * sizeof p);

			nrgpuproc--;
			g--;
		}
	}

	/*
 	** merge gpu stats with sorted task list of active processes
	*/
	for (t=0; t < ntaskpres && gpuleft > 0; t++)
	{
		if (curtpres[t].gen.isproc)
		{
			for (g=0; g < nrgpuproc; g++)
			{
				if (gpp[g] == NULL)
					continue;
				if (curtpres[t].gen.pid != gpp[g]->pid)
					continue;
				curtpres[t].gpu = gpp[g]->gpu;
				gpp[g] = NULL;

				if (--gpuleft == 0)
					break;
			}
		}
	}

	if (gpuleft == 0)
	{
		free(gpp);
		return;
	}

	/*
 	** compact list with pointers to remaining pids
	*/
	for (g=t=0; g < nrgpuproc; g++)
	{
		if (gpp[g] == NULL)
		{
			for (; t < nrgpuproc; t++)
			{
				if (gpp[t])
				{
					gpp[g] = gpp[t];
					gpp[t] = NULL;
					break;
				}
			}
		}
	}

	/*
 	** merge remaining gpu stats with task list of exited processes
	*/
	for (t=0; t < nprocexit && gpuleft; t++)
	{
		if (curpexit[t].gen.isproc)
		{
			for (g=0; g < gpuleft; g++)
			{
				if (gpp[g] == NULL)
					continue;

				if (curpexit[t].gen.pid == gpp[g]->pid)
				{
					curpexit[t].gpu = gpp[g]->gpu;
					gpp[g] = NULL;
					gpuleft--;
				}
			}
		}
	}

	free(gpp);
}
