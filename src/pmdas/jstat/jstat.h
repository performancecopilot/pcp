/*
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
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

#define JSTAT_COMMAND "jstat -J-Djstat.showUnsupported=true -snap "

typedef struct {
    char		*command;	/* jstat command line */
    char		*name;		/* symbolic instance name */
    char		*file;		/* jstat output file */
    FILE		*fin;		/* read output here */
    struct stat		pidstat;	/* on /var/tmp/jstat/x.pid */
    int			pid;		/* process ID for instance */
    int			error;		/* error on this instance */

    __uint32_t		contended_lock_attempts;
    __uint32_t		deflations;
    __uint32_t		futile_wakeups;
    __uint32_t		inflations;
    __uint32_t		notifications;
    __uint32_t		parks;

    int			fetched;	/* initial values retrieved */
} jstat_t;

extern jstat_t		*jstat;
extern int		jstat_count;

#define JSTAT_INDOM	0
#define ACTIVE_INDOM	1
extern pmdaIndom	indomtab[];
extern pmdaInstid	*jstat_insts;

#endif /* JSTAT_H */
