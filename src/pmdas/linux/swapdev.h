/*
 * Linux Filesystem Cluster
 *
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

#ident "$Id: swapdev.h,v 1.2 1999/03/10 02:38:24 markgw Exp $"

typedef struct {
    int		  id;
    int           valid;
    int           seen;
    char	  *path;
    unsigned int  size;
    unsigned int  used;
    int		  priority;
} swapdev_entry_t;

typedef struct {
    int		  nswaps;
    swapdev_entry_t *swaps;
    pmdaIndom *indom;
} swapdev_t;

extern int refresh_swapdev(swapdev_t *);
