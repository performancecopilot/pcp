/*
 * Copyright (c) 2012-2013 Red Hat.
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
 */
#ifndef _CLIENT_H
#define _CLIENT_H

struct __pmSockAddr;

/* The table of clients, used by pmcd */
typedef struct {
    int			fd;		/* Socket descriptor */
    struct {				/* Status of connection to client */
	unsigned int	connected : 1;	/* Client connected */
	unsigned int	changes : 3;	/* PMCD_* bits for changes since last fetch */
    } status;
    /* There is an array of profiles, as there is a profile associated
     * with each client context.  The array is not guaranteed to be dense.
     * The context number sent with each profile/fetch is used to index it.
     */
    __pmProfile		**profile;	/* Client context profile pointers */
    int			szProfile;	/* Size of array */
    unsigned int	denyOps;	/* Disallowed operations for client */
    __pmPDUInfo		pduInfo;
    unsigned int	seq;		/* client sequence number */
    time_t		start;		/* time client connected */
    struct __pmSockAddr	*addr;		/* Address of client */
} ClientInfo;

PMCD_EXTERN ClientInfo	*client;		/* Array of clients */
PMCD_EXTERN int		nClients;		/* Number of entries in array */
extern int		maxClientFd;		/* largest fd for a client */
extern __pmFdSet	clientFds;		/* for client select() */
PMCD_EXTERN int		this_client_id;		/* client for current request */

/* prototypes */
extern ClientInfo *AcceptNewClient(int);
extern int NewClient(void);
extern void DeleteClient(ClientInfo *);
PMCD_EXTERN void ShowClients(FILE *m);

#ifdef PCP_DEBUG
extern char *nameclient(int);
#endif

#endif /* _CLIENT_H */
