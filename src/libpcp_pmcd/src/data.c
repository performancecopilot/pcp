/*
 * Copyright (c) 2012,2015 Red Hat.
 * Copyright (c) 1995-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "pmcd.h"

/*
 * Global data shared by pmcd and the pmcd PMDA DSO must reside
 * in a DSO as well, due to linkage oddities with Windows DLLs.
 */

PMCD_DATA int	pmcd_hi_openfds = -1;   /* Highest open pmcd file descriptor */
PMCD_DATA int	_pmcd_done;		/* flag from pmcd pmda */
PMCD_DATA int	_pmcd_timeout = 5;	/* Timeout for hung agents */

PMCD_DATA int	nAgents;		/* Number of active agents */
PMCD_DATA AgentInfo *agent;		/* Array of agent info structs */

PMCD_DATA char *_pmcd_hostname;		/* Explicitly requested hostname */

/*
 * File descriptors are used as an internal index with the advent
 * of NSPR in libpcp.  We (may) need to first decode the index to
 * an internal representation and lookup the real file descriptor.
 * Note the use of on-stack fd overwrite, avoiding local variable.
 */

void
pmcd_openfds_sethi(int fd)
{
    if ((fd = __pmFD(fd)) > pmcd_hi_openfds)
	pmcd_hi_openfds = fd;
}
