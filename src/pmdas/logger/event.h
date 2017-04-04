/*
 * Event support for the Logger PMDA
 *
 * Copyright (c) 2011 Red Hat Inc.
 * Copyright (c) 2011 Nathan Scott.  All rights reversed.
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
#include <sys/stat.h>

typedef struct event_logfile {
    pmID		pmid;
    int			fd;
    pid_t	        pid;
    int			queueid;
    int			noaccess;
    struct stat		pathstat;
    char		pmnsname[MAXPATHLEN];
    char		pathname[MAXPATHLEN];
} event_logfile_t;

extern int maxfd;
extern fd_set fds;
extern long maxmem;

extern void event_init(pmID pmid);
extern void event_shutdown(void);
extern void event_refresh(void);
extern int event_config(const char *filename);

extern int event_logcount(void);
extern pmID event_pmid(int handle);
extern int event_queueid(int handle);
extern __uint64_t event_pathsize(int handle);
extern const char *event_pathname(int handle);
extern const char *event_pmnsname(int handle);
extern int event_decoder(int arrayid, void *buffer, size_t size,
			 struct timeval *timestamp, void *data);
extern int event_regex_alloc(const char *s, void **filter);
extern int event_regex_apply(void *rp, void *data, size_t size);
extern void event_regex_release(void *rp);

#endif /* _EVENT_H */
