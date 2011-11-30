/*
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "pmcd.h"

/*
 * Global data shared by pmcd and the pmcd PMDA DSO must reside
 * in a DSO as well, due to linkage oddities with Windows DLLs.
 */

PMCD_INTERN int	pmcd_hi_openfds = -1;   /* Highest open pmcd file descriptor */
PMCD_INTERN int	_pmcd_done;		/* flag from pmcd pmda */
PMCD_INTERN int	_pmcd_timeout = 5;	/* Timeout for hung agents */

PMCD_INTERN AgentInfo *agent;		/* Array of agent info structs */
PMCD_INTERN int	nAgents;		/* Number of active agents */

