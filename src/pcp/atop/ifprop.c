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

#include "atop.h"
#include "ifprop.h"
#include "photosyst.h"
#include "ifpropmetrics.h"

static struct ifprop	*ifprops;

#define BTOMBIT(bytes) ((bytes)/125000)	/* bytes/second to Mbits/second */

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
** function compares two interface names for struct sorting
 */
static int
cmpifprop(const void *a, const void *b)
{
    struct ifprop	*ifa = (struct ifprop *)a;
    struct ifprop	*ifb = (struct ifprop *)b;

    return strcmp(ifa->name, ifb->name);
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
	count_t		speed;
	size_t		propsize;
	char		**insts;
	int		sts, i;
	int		*ids, count;
	struct ifprop	*new_ifprops;

	if (!setup)
	{
		setup_metrics(ifpropmetrics, pmids, descs, IF_NMETRICS);
		setup = 1;
	}

	fetch_metrics("ifprop", IF_NMETRICS, pmids, &result);

	/* extract external interface names */
	count = get_instances("ifprop", IF_SPEED, descs, &ids, &insts);

	propsize = (count + 1) * sizeof ifprops[0];
	if ((new_ifprops = realloc(ifprops, propsize)) == NULL)
	{
		fprintf(stderr,
			"%s: allocating interface table: %s [%ld bytes]\n",
			pmGetProgname(), strerror(errno), (long)propsize);
		cleanstop(1);
		/* NOTREACHED */
	}
	ifprops = new_ifprops;

	for (i=0; i < count; i++)
	{
		struct ifprop	*ip = &ifprops[i];

		pmstrncpy(ip->name, MAXINTNM, insts[i]);

		/* extract duplex/speed from result for given inst id */
		speed = extract_count_t_inst(result, descs, IF_SPEED, ids[i], i);
		ip->speed = speed < 0 ? 0 : BTOMBIT(speed); /* Mbits/second */
		sts = extract_integer_inst(result, descs, IF_DUPLEX, ids[i], i);
		ip->fullduplex = sts < 0 ? 0 : sts;

		sts = extract_integer_inst(result, descs, IF_VIRTUAL, ids[i], i);
		if (sts == 1)
		{
			ip->type = 'v';
			continue;
		}
		
		sts = extract_integer_inst(result, descs, IF_WIRELESS, ids[i], i);
		if (sts == 1)
			ip->type = 'w';
		else
		{
			sts = extract_integer_inst(result, descs, IF_TYPE, ids[i], i);
			if (sts == 0)
				ip->type = 'w';
			else if (sts == 1)
				ip->type = 'e';
			else
				ip->type = '?';
		}
	}
	qsort(ifprops, count, sizeof *ifprops, cmpifprop);
	ifprops[i].name[0] = '\0';
	pmFreeResult(result);
	free(insts);
	free(ids);
}
