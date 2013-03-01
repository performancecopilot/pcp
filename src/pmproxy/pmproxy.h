/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef _PROXY_H
#define _PROXY_H

#include "pmapi.h"
#include "impl.h"

/* The table of clients, used by pmproxy */
typedef struct {
    int			fd;		/* client socket descriptor */
    int			version;	/* proxy-client protocol version */
    struct {				/* Status of connection to client */
	unsigned int	connected : 1;	/* Client connected, socket level */
	unsigned int	allowed : 1;	/* Creds seen, OK to talk to pmcd */
    } status;
    char		*pmcd_hostname;	/* PMCD hostname */
    int			pmcd_port;	/* PMCD port */
    int			pmcd_fd;	/* PMCD socket file descriptor */
    __pmSockAddr	*addr;		/* address of client */
} ClientInfo;

extern ClientInfo	*client;	/* Array of clients */
extern int		nClients;	/* Number of entries in array */
extern int		maxReqPortFd;	/* highest request port fd */
extern int		maxSockFd;	/* largest fd for a clients
					 * and pmcd connections */
extern __pmFdSet	sockFds;	/* for select() */

/* prototypes */
extern ClientInfo *AcceptNewClient(int);
extern void DeleteClient(ClientInfo *);
extern void StartDaemon(int, char **);
extern void Shutdown(void);

#endif /* _PROXY_H */
