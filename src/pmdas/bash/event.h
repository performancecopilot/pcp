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

#include <sys/types.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"

typedef struct bash_process {
    int			fd;
    pid_t	        pid;
    pid_t	        parent;
    int			queueid;

    int			exited  : 1;	/* flag: process running? */
    int			finished: 1;	/* flag: exit event sent? */
    int			noaccess: 1;	/* flag: store-to-access? */
    int			version : 8;	/* pmda <-> bash xtrace version */
    int			padding : 21;	/* filler */

    struct timeval	starttime;	/* timestamp trace started */
    struct timeval	startstat;	/* timestamp of first stat */
    struct stat		stat;		/* stat of the header file */

    char		*instance;	/* process id, space, script */
} bash_process_t;

typedef struct bash_trace {
    int			flags;
    struct timeval	timestamp;
    int			line;
    char		function[64];
    char		command[512];
} bash_trace_t;

extern long bash_maxmem;

extern void event_init(void);
extern void event_refresh(pmInDom);
extern int event_start(bash_process_t *, struct timeval *);
extern void process_stat_timestamp(bash_process_t *, struct timeval *);

#define MINIMUM_VERSION	1
#define MAXIMUM_VERSION	1

extern int extract_int(char *, const char *, size_t, int *);
extern int extract_str(char *, size_t, const char *, size_t, char *, size_t);
extern int extract_cmd(char *, size_t, const char *, size_t, char *, size_t);

#endif /* _EVENT_H */
