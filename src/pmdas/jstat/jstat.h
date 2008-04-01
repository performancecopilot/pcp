/*
 * Copyright (c) 2007-2008 Aconex.  All Rights Reserved.
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
 */
#ifndef JSTAT_H
#define JSTAT_H

#include <pmapi.h>
#include <impl.h>
#include <pmda.h>
#include "./domain.h"

#define JSTAT_COMMAND	"jstat -J-Djstat.showUnsupported=true -snap "
#define BUFFER_MAXLEN	4096

typedef struct {
    char		*command;	/* jstat command line */
    char		*name;		/* symbolic instance name */
    int			pid;		/* process ID for instance */

    __int64_t		contended_lock_attempts;
    __int64_t		deflations;
    __int64_t		futile_wakeups;
    __int64_t		inflations;
    __int64_t		notifications;
    __int64_t		parks;

    __int64_t		minor_gc_count;
    __int64_t		minor_gc_time;
    __int64_t		major_gc_count;
    __int64_t		major_gc_time;

    __int64_t		eden_capacity;
    __int64_t		eden_init_capacity;
    __int64_t		eden_max_capacity;
    __int64_t		eden_used;
    __int64_t		survivor0_capacity;
    __int64_t		survivor0_init_capacity;
    __int64_t		survivor0_max_capacity;
    __int64_t		survivor0_used;
    __int64_t		survivor1_capacity;
    __int64_t		survivor1_init_capacity;
    __int64_t		survivor1_max_capacity;
    __int64_t		survivor1_used;
    __int64_t		old_capacity;
    __int64_t		old_init_capacity;
    __int64_t		old_max_capacity;
    __int64_t		old_used;
    __int64_t		permanent_capacity;
    __int64_t		permanent_init_capacity;
    __int64_t		permanent_max_capacity;
    __int64_t		permanent_used;

    int			fetched;	/* initial values retrieved */
} jstat_t;

extern jstat_t		*jstat;
extern int		jstat_count;

#define JSTAT_INDOM	0
#define ACTIVE_INDOM	1
extern pmdaIndom	indomtab[];
extern pmdaInstid	*jstat_insts;

#endif /* JSTAT_H */
