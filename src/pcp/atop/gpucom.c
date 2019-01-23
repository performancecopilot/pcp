/*
** Copyright (C) 2018 Gerlof Langeveld <gerlof.langeveld@atoptool.nl>
** Copyright (C) 2019 Red Hat.
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
#include "gpucom.h"

#if 0
static char	**gpubusid;	// array with char* to busid strings
static char	**gputypes;	// array with char* to type strings
static char	*gputasks;	// array with chars with tasksupport booleans

/*
** Parse response string from server on 'T' request
**
** Store the type, busid and tasksupport of every GPU in
** static pointer tables
*/
static void
gputype_parse(char *buf)
{
	char	*p, *start, **bp, **tp, *cp;

	/*
	** parse GPU info and build arrays of pointers to the
	** busid strings, type strings and tasksupport strings.
	*/
	if (numgpus)			// GPUs present anyhow?
	{
		int field;

		gpubusid = bp = malloc((numgpus+1) * sizeof(char *));
		gputypes = tp = malloc((numgpus+1) * sizeof(char *));
		gputasks = cp = malloc((numgpus)   * sizeof(char  ));

		ptrverify(gpubusid, "Malloc failed for gpu busids\n");
		ptrverify(gputypes, "Malloc failed for gpu types\n");
		ptrverify(gputasks, "Malloc failed for gpu tasksup\n");

		for (field=0, start=p; ; p++)
		{
			if (*p == ' ' || *p == '\0' || *p == GPUDELIM)
			{
				switch(field)
				{
				   case 0:
					if (p-start <= MAXGPUBUS)
						*bp++ = start;
					else
						*bp++ = p - MAXGPUBUS;
					break;
				   case 1:
					if (p-start <= MAXGPUTYPE)
						*tp++ = start;
					else
						*tp++ = p - MAXGPUTYPE;
					break;
				   case 2:
					*cp++ = *start;
					break;
				}

				field++;
				start = p+1;

				if (*p == '\0')
					break;

				if (*p == GPUDELIM)
					field = 0;

				*p = '\0';
			}
		}

		*bp = NULL;
		*tp = NULL;
	}
}

/*
** Parse GPU statistics string
*/
static void
gpuparse(int version, char *p, struct pergpu *gg)
{
	switch (version)
	{
	   case 1:
		(void) sscanf(p, "%d %d %lld %lld %lld %lld %lld %lld", 
			&(gg->gpupercnow), &(gg->mempercnow),
			&(gg->memtotnow),  &(gg->memusenow),
			&(gg->samples),    &(gg->gpuperccum),
			&(gg->memperccum), &(gg->memusecum));

		gg->nrprocs = 0;

		break;
	}
}


/*
** Parse PID statistics string
*/
static void
pidparse(int version, char *p, struct gpupidstat *gp)
{
	switch (version)
	{
	   case 1:
		(void) sscanf(p, "%c %ld %d %d %lld %lld %lld %lld",
			&(gp->gpu.state),   &(gp->pid),    
			&(gp->gpu.gpubusy), &(gp->gpu.membusy),
			&(gp->gpu.timems),
			&(gp->gpu.memnow), &(gp->gpu.memcum),
		        &(gp->gpu.sample));
		break;
	}
}
#endif

/*
** Merge the GPU per-process counters with the other
** per-process counters
*/
static int compgpupid(const void *, const void *);

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
	gpp = malloc(nrgpuproc * sizeof(struct gpupidstat *));
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
	for (t=g=0; t < ntaskpres && g < nrgpuproc; t++)
	{
		if (curtpres[t].gen.isproc)
		{
			if (curtpres[t].gen.pid == gpp[g]->pid)
			{
				curtpres[t].gpu = gpp[g]->gpu;
				gpp[g++] = NULL;

				if (--gpuleft == 0 || g >= nrgpuproc)
					break;
			}

			// anyhow resync
			while ( curtpres[t].gen.pid > gpp[g]->pid)
			{
				if (++g >= nrgpuproc)
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

static int
compgpupid(const void *a, const void *b)
{
	return (*((struct gpupidstat **)a))->pid - (*((struct gpupidstat **)b))->pid;
}
