/*
 * Linux Filesystem Cluster
 *
 * Copyright (c) 2000,2004,2007 Silicon Graphics, Inc.  All Rights Reserved.
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

#ident "$Id: filesys.h,v 1.6 2007/12/18 04:29:10 kimbrr Exp $"

#include <sys/vfs.h>

typedef struct {
    int32_t	  space_time_left;	/* seconds */
    int32_t	  files_time_left;	/* seconds */
    uint64_t	  space_hard;		/* blocks */
    uint64_t	  space_soft;		/* blocks */
    uint64_t	  space_used;		/* blocks */
    uint64_t	  files_hard;
    uint64_t	  files_soft;
    uint64_t	  files_used;
} quota_entry_t;


/* Values for flags in filesys_entry_t */
#define FSF_FETCHED		(1U << 0)
#define FSF_VALID		(1U << 1)
#define FSF_SEEN		(1U << 2)
#define FSF_QUOT_PROJ_ACC	(1U << 3)
#define FSF_QUOT_PROJ_ENF	(1U << 4)

typedef struct {
    int		  id;
    unsigned	  flags;
    char	  *device;
    char	  *path;
    struct statfs stats;
} filesys_entry_t;

typedef struct {
    int		  nmounts;
    filesys_entry_t *mounts;
    pmdaIndom 	*indom;
} filesys_t;

extern int refresh_filesys(filesys_t *filesys, pmInDom qindom);
