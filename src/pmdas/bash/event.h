/*
 * Event support for the bash tracing PMDA
 *
 * Copyright (c) 2012 Nathan Scott.  All rights reversed.
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
#ifndef _EVENT_H
#define _EVENT_H

#include "pmapi.h"
#include "impl.h"

typedef struct bash_process {
    int			fd;
    int			queueid;

    int			first   : 1;	/* flag: first time seen? */
    int			exited  : 1;	/* flag: process running? */
    int			restrict: 1;	/* flag: store-to-access? */
    int			padding : 29;	/* filler */

    pid_t	        pid;
    pid_t	        parent;
    int			line;
    int			time;	/* seconds */
    int			date;	/* ??? */

    struct timeval	starttime;
    struct stat		stat;

    char		*script;
    char		*function;
    char		*command;
    char		basename[1];
} bash_process_t;

extern long bash_maxmem;

extern void event_init(void);
extern void event_refresh(pmInDom indom);

#endif /* _EVENT_H */
