/*
 * Linux /proc/partitions metrics cluster
 *
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#ident "$Id: proc_partitions.h,v 1.11 2005/06/07 07:18:20 markgw Exp $"

typedef struct {
    int			id;
    unsigned int	major;
    unsigned int	minor;
    unsigned long	nr_blocks;
    char		*namebuf;     /* from /proc/{partitions,diskstats} */
    char		*udevnamebuf; /* from udev if we have it, else NULL */
    unsigned int	rd_ios;
    unsigned int	rd_merges;
    unsigned int	rd_sectors;
    unsigned int	rd_ticks;
    unsigned int	wr_ios;
    unsigned int	wr_merges;
    unsigned int	wr_sectors;
    unsigned int	wr_ticks;
    unsigned int	ios_in_flight;
    unsigned int	io_ticks;
    unsigned int	aveq;
} partitions_entry_t;

extern int refresh_proc_partitions(pmInDom disk_indom, pmInDom partitions_indom);
extern int is_partitions_metric(pmID);
extern int proc_partitions_fetch(pmdaMetric *, unsigned int, pmAtomValue *);
