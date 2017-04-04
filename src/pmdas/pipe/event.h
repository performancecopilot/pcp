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
    char		*user;	/* run command as this user */
    int			inst;	/* internal instance ID */
} pipe_command;

/* per-client-context structures */
typedef struct pipe_groot {
	int		fd;	/* command output pipe fd */
	int		pid;	/* process ID of command */
	int		inst;	/* internal instance ID */
	int		count;	/* count of piped lines */
	int		active;	/* is command running? */
	int		exited;	/* has command exited? */
	int		status;	/* commands exit status */
	int		queueid;	/* event queue ID */
	char		qname[64];	/* event queue name */
} pipe_groot;

typedef struct pipe_client {
    char		*uid;	/* authenticated user ID */
    char		*gid;	/* authenticated group ID */
    struct pipe_groot	*pipes;
} pipe_client;

typedef struct pipe_acl {
    char	*identifier;	/* pipe instance name */
    char	*name;		/* user or group name */
    int		operation;	/* instance operation ID */
    int		disallow : 1;
    int		allow : 1;
    int		user : 1;
    int		group : 1;
} pipe_acl;

extern size_t maxmem;
extern long numcommands;
extern pmID *paramline;

extern void event_acl(pmInDom);
extern void event_indom(pmInDom);

extern int event_init(int, pmInDom, pipe_command *, char *);
extern void event_client_access(int);
extern void event_client_shutdown(int);
extern void event_child_shutdown(void);
extern void event_capture(fd_set *);
extern int event_config(const char *);
extern int event_config_dir(const char *);
extern int event_queueid(int, unsigned int);
extern int event_qactive(int, unsigned int);
extern void *event_qdata(int, unsigned int);
extern int event_decoder(int, void *, size_t, struct timeval *, void *);
extern int event_groupid(int, const char *);
extern int event_userid(int, const char *);

extern int pipe_setfd(int);
extern int pipe_clearfd(int);

#endif /* EVENT_H */
