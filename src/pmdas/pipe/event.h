/*
 * Copyright (c) 2015 Red Hat.
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
#ifndef EVENT_H
#define EVENT_H

#include "pmapi.h"
#include "impl.h"

/* per-configured-pipe structure */
typedef struct pipe_command {
    char		*identifier;
    char		*command;
    char		*user;
    int			inst;	/* internal instance ID */
    int			uid;	/* run command as this user */
} pipe_command;

/* per-client-context structure */
typedef struct pipe_client {
    int			uid;	/* authenticated user ID */
    int			gid;	/* authenticated group ID */
    struct pipe_groot {
	int		fd;	/* command output pipe fd */
	int		pid;	/* process ID of command */
	int		inst;	/* internal instance ID */
	int		count;	/* count of piped lines */
	int		active;	/* is command running? */
	int		exited;	/* has command exited? */
	int		status;	/* commands exit status */
	int		queueid;	/* event queue ID */
	char		qname[64];	/* event queue name */
    } pipes[0];
} pipe_client;

extern size_t maxmem;
extern long numcommands;
extern pmID *paramline;

extern int event_init(int, pipe_command *, char *);
extern void event_client_access(int);
extern void event_client_shutdown(int);
extern void event_child_shutdown(void);
extern void event_capture(fd_set *);
extern int event_config(const char *);
extern int event_config_dir(const char *);
extern void event_indom(pmInDom);
extern int event_queueid(int, unsigned int);
extern int event_qactive(int, unsigned int);
extern void *event_qdata(int, unsigned int);
extern int event_decoder(int, void *, size_t, struct timeval *, void *);

extern int pipe_setfd(int);
extern int pipe_clearfd(int);

#endif /* EVENT_H */
