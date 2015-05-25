/*
** ATOP - System & Process Monitor 
** 
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
** 
** This source-file contains all functions required to manipulate the
** process-database. This database is implemented as a linked list of
** all running processes, needed to remember the process-counters from
** the previous sample.
**
** Copyright (C) 2000-2012 Gerlof Langeveld
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

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

#include "atop.h"
#include "photoproc.h"

/*****************************************************************************/
#define	NPHASH	256		/* number of hash queues for process dbase   */
				/* MUST be a power of 2 !!!                  */

	/* hash buckets for getting process-info     */
	/* for a given PID 			     */
static struct pinfo	*phash[NPHASH];

	/* cyclic list of all processes, to detect   */
	/* which processes were not referred	     */
static struct pinfo	presidue;
/*****************************************************************************/


/*
** search process database for the given PID
*/
int
pdb_gettask(int pid, char isproc, time_t btime, struct pinfo **pinfopp)
{
	register struct pinfo	*pp;

	pp = phash[pid&(NPHASH-1)];	/* get proper hash bucket	*/

	/*
	** scan all entries in hash Q
	*/
	while (pp)
	{
		/*
		** if this is required PID, unchain it from the RESIDUE-list
		** and return info
		*/
		if (pp->tstat.gen.pid    == pid    && 
		    pp->tstat.gen.isproc == isproc   )
		{
			int diff = pp->tstat.gen.btime - btime;

			/*
			** with longer intervals, the same PID might be
			** found more than once, so also check the start
			** time of the task
			*/
			if (diff > 1 || diff < -1)
			{
				pp = pp->phnext;
				continue;
			}

			if (pp->prnext)		/* if part of RESIDUE-list   */
			{
				(pp->prnext)->prprev = pp->prprev; /* unchain */
				(pp->prprev)->prnext = pp->prnext;
			}

			pp->prnext = NULL;
			pp->prprev = NULL;

			*pinfopp = pp;

			return 1;
		}

		pp = pp->phnext;
	}

	/*
	** end of list; PID not found
	*/
	return 0;
}

/*
** add new process-info structure to the process database
*/
void
pdb_addtask(int pid, struct pinfo *pinfop)
{
	register int i	= pid&(NPHASH-1);

	pinfop->phnext 	= phash[i];
	phash[i] 	= pinfop;
}

/*
** delete a process from the process database
*/
int
pdb_deltask(int pid, char isproc)
{
	register struct pinfo	*pp, *ppp;

	pp = phash[pid&(NPHASH-1)];	/* get proper hash bucket	*/

	/*
	** check first entry in hash Q
	*/
	if (pp->tstat.gen.pid == pid && pp->tstat.gen.isproc == isproc)
	{
		phash[pid&(NPHASH-1)] = pp->phnext;

		if ( pp->prnext )	/* still part of RESIDUE-list ? */
		{
			(pp->prprev)->prnext = pp->prnext;
			(pp->prnext)->prprev = pp->prprev;	/* unchain */
		}

		/*
		** remove process-info from process-database
		*/
		free(pp);

		return 1;
	}

	/*
	** scan other entries of hash-list
	*/
	ppp	= pp;
	pp	= pp->phnext;

	while (pp)
	{
		/*
		** if this is wanted PID, unchain it from the RESIDUE-list
		** and return info
		*/
		if (pp->tstat.gen.pid == pid && pp->tstat.gen.isproc == isproc)
		{
			ppp->phnext = pp->phnext;

			if ( pp->prnext )	/* part of RESIDUE-list ? */
			{
				(pp->prnext)->prprev = pp->prprev;
				(pp->prprev)->prnext = pp->prnext;
			}

			/*
			** remove process-info from process-database
			*/
			free(pp);

			return 1;
		}

		ppp	= pp;
		pp	= pp->phnext;
	}

	return 0;	/* PID not found */
}

/*
** Chain all process-info structures into the RESIDUE-list;
** every process-info struct which is referenced later on by pdb_gettask(),
** will be removed from this list again. After that, the remaining
** (unreferred) process-info structs can be easily discovered and
** eventually removed.
*/
int
pdb_makeresidue(void)
{
	register struct pinfo	*pp, *pr;
	register int		i;

	/*
	** prepare RESIDUE-list anchor
	*/
	pr = &presidue;

	pr->prnext	= pr;
	pr->prprev	= pr;

	/*
	** check all entries in hash list
	*/
	for (i=0; i < NPHASH; i++)
	{
		if (!phash[i])
			continue;	/* empty hash bucket */

		pp = phash[i];		/* get start of list */

		while (pp)		/* all entries in hash list	*/
		{
			pp->prnext		= pr->prnext;
			pr->prnext		= pp;

			 pp->prprev		= (pp->prnext)->prprev;
			(pp->prnext)->prprev	= pp;

			pp = pp->phnext;	/* get next of hash list */
		}
	}

	/*
	** all entries chained in doubly-linked RESIDUE-list
	*/
	return 1;
}

/*
** remove all remaining entries in RESIDUE-list
*/
int
pdb_cleanresidue(void)
{
	register struct pinfo	*pr;
	register int		pid;
        char			isproc;

	/*
	** start at RESIDUE-list anchor and delete all entries
	*/
	pr = presidue.prnext;

	while (pr != &presidue)
	{
		pid    = pr->tstat.gen.pid;
		isproc = pr->tstat.gen.isproc;

		pr  = pr->prnext;	/* MUST be done before deletion */

		pdb_deltask(pid, isproc);
	}

	return 1;
}

/*
** search in the RESIDUE-list for process-info which may fit to the
** given process-info, for which the PID is not known
*/
int
pdb_srchresidue(struct tstat *tstatp, struct pinfo **pinfopp)
{
	register struct pinfo	*pr, *prmin=NULL;
	register long		btimediff;

	/*
	** start at RESIDUE-list anchor and search through
	** all remaining entries
	*/
	pr = presidue.prnext;

	while (pr != &presidue)	/* still entries left ? */
	{
		/*
		** check if this entry matches searched info
		*/
		if ( 	pr->tstat.gen.ruid   == tstatp->gen.ruid	&& 
			pr->tstat.gen.rgid   == tstatp->gen.rgid	&& 
			strcmp(pr->tstat.gen.name, tstatp->gen.name) == EQ  )
		{
			/*
			** check if the start-time of the process is exactly
			** the same ----> then we have a match;
			** however sometimes the start-time may deviate a
			** second although it IS the process we are looking
			** for (depending on the rounding of the boot-time),
			** so if we don't find the exact match, we will check
			** later on if we found an almost-exact match
			*/
			btimediff = pr->tstat.gen.btime - tstatp->gen.btime;

			if (btimediff == 0)	/* gotcha !! */
			{
				*pinfopp = pr;
				return 1;
			}

			if ((btimediff== -1 || btimediff== 1) && prmin== NULL)
				prmin = pr;	/* remember this process */
		}

		pr = pr->prnext;
	}

	/*
	** nothing found that matched exactly;
	** do we remember a process that matched almost exactly?
	*/
	if (prmin)
	{
		*pinfopp = prmin;
		return 1;
	}

	return 0;	/* even not almost */
}
