/*
 * Linux /proc/net/dev metrics cluster
 *
 * Copyright (c) 1995,2005 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: proc_net_dev.h,v 1.6 2005/06/06 08:27:45 kenmcd Exp $"

#define PROC_DEV_COUNTERS_PER_LINE   16

typedef struct {
    uint64_t	last_gen;
    uint64_t	last_counters[PROC_DEV_COUNTERS_PER_LINE];
    uint64_t	counters[PROC_DEV_COUNTERS_PER_LINE];
} net_interface_t;

extern int refresh_proc_net_dev(pmInDom);
