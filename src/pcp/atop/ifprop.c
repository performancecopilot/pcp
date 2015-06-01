/*
** ATOP - System & Process Monitor
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** Copyright (C) 2015 Red Hat.
** Copyright (C) 2007-2010 Gerlof Langeveld
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

#include "atop.h"
#include "ifprop.h"
#include "photosyst.h"
#include "ifpropmetrics.h"

static struct ifprop	*ifprops;

/*
** function searches for the properties of a particular interface
** the interface name should be filled in the struct ifprop before
** calling this function
**
** return value reflects true or false
*/
int
getifprop(struct ifprop *ifp)
{
	register int	i;

	for (i=0; ifprops[i].name[0]; i++)
	{
		if (strcmp(ifp->name, ifprops[i].name) == 0)
		{
			*ifp = ifprops[i];
			return 1;
		}
	}

	ifp->speed	= 0;
	ifp->fullduplex	= 0;

	return 0;
}

/*
** function stores properties of all interfaces in a static
** table to be queried later on
*/
void
initifprop(void)
{
	static int	setup;
	pmID		pmids[IF_NMETRICS];
	pmDesc		descs[IF_NMETRICS];
	pmResult	*result;
	char		**insts;
	int		sts, i;
	int		*id, count;

	if (!setup)
	{
		setup_metrics(ifpropmetrics, pmids, descs, IF_NMETRICS);
		setup = 1;
	}

	pmSetMode(fetchmode, &curtime, fetchstep);
	if ((sts = pmFetch(IF_NMETRICS, pmids, &result)) < 0)
	{
		fprintf(stderr, "%s: pmFetch: fetching interface values: %s\n",
			pmProgname, pmErrStr(sts));
		cleanstop(1);
	}
	if (IF_NMETRICS != result->numpmid)
	{
		fprintf(stderr,
			"%s: pmFetch: failed fetching some interface values\n",
			pmProgname);
		cleanstop(1);
	}

	/* extract external interface names */
	if (rawreadflag)
	    sts = pmGetInDomArchive(descs[IF_SPEED].indom, &id, &insts);
	else
	    sts = pmGetInDom(descs[IF_SPEED].indom, &id, &insts);
	if (sts < 0)
	{
		fprintf(stderr,
			"%s: pmGetInDom: failed on interface instances: %s\n",
			pmProgname, pmErrStr(sts));
		cleanstop(1);
	}
	count = sts;

	if ((ifprops = calloc(count + 1, sizeof(ifprops[0]))) == NULL)
	{
		fprintf(stderr,
			"%s: allocating interface table: %s\n",
			pmProgname, strerror(errno));
		cleanstop(1);
	}

	for (i=0; i < count; i++)
	{
		struct ifprop	*ip = &ifprops[i];

		strncpy(ip->name, insts[i], MAXINTNM-1);
		ip->name[MAXINTNM-1] = '\0';

		/* extract duplex/speed from result for given inst id */
		sts = extract_integer_inst(result, descs, IF_SPEED, id[i]);
		ip->speed = sts < 0 ? 0 : sts * 8;
		sts = extract_integer_inst(result, descs, IF_DUPLEX, id[i]);
		ip->fullduplex = sts < 0 ? 0 : sts;
	}
	pmFreeResult(result);
	free(insts);
	free(id);
}
