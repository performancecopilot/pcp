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
	size_t		propsize;
	char		**insts;
	int		sts, i;
	int		*ids, count;

	if (!setup)
	{
		setup_metrics(ifpropmetrics, pmids, descs, IF_NMETRICS);
		setup = 1;
	}

	fetch_metrics("ifprop", IF_NMETRICS, pmids, &result);

	/* extract external interface names */
	count = get_instances("ifprop", IF_SPEED, descs, &ids, &insts);

	propsize = (count + 1) * sizeof(ifprops[0]);
	if ((ifprops = realloc(ifprops, propsize)) == NULL)
	{
		fprintf(stderr,
			"%s: allocating interface table: %s [%ld bytes]\n",
			pmProgname, strerror(errno), (long)propsize);
		cleanstop(1);
	}

	for (i=0; i < count; i++)
	{
		struct ifprop	*ip = &ifprops[i];

		strncpy(ip->name, insts[i], MAXINTNM-1);
		ip->name[MAXINTNM-1] = '\0';

		/* extract duplex/speed from result for given inst id */
		sts = extract_integer_inst(result, descs, IF_SPEED, ids[i]);
		ip->speed = sts < 0 ? 0 : sts * 8;
		sts = extract_integer_inst(result, descs, IF_DUPLEX, ids[i]);
		ip->fullduplex = sts < 0 ? 0 : sts;
	}
	ifprops[i].name[0] = '\0';
	pmFreeResult(result);
	free(insts);
	free(ids);
}
