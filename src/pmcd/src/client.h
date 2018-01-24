/*
 * Copyright (c) 2012-2018 Red Hat.
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
#ifndef PMCD_CLIENT_H
#define PMCD_CLIENT_H

/* The table of clients, used by pmcd */
typedef struct {
    int			fd;		/* Socket descriptor */
    struct {				/* Status of connection to client */
	unsigned int	connected : 1;	/* Client connected */
	unsigned int	changes : 6;	/* PMCD_* bits for changes since last fetch */
	unsigned int	attributes: 1;	/* Connection attributes have changed */
    } status;
    /* There is a profile associated with each client context.
     * The context slot number (not the context number) sent with each
     * profile/fetch is used as the key to the profile hash table.
     */
    __pmHashCtl		profile;	/* Client context profile pointers */
    unsigned int	denyOps;	/* Disallowed operations for client */
    __pmPDUInfo		pduInfo;
    unsigned int	seq;		/* Client sequence number (pmdapmcd) */
    time_t		start;		/* Time client connected (pmdapmcd) */
    __pmSockAddr	*addr;		/* Network address of client */
    __pmHashCtl		attrs;		/* Connection attributes (tuples) */
} ClientInfo;

PMCD_DATA extern ClientInfo *client;		/* Array of clients */
PMCD_DATA extern int	nClients;		/* Number of entries in array */
extern int		maxClientFd;		/* largest fd for a client */
extern __pmFdSet	clientFds;		/* for client select() */
PMCD_DATA extern int	this_client_id;		/* client for current request */

/* prototypes */
extern ClientInfo *AcceptNewClient(int);
extern int NewClient(void);
extern void DeleteClient(ClientInfo *);
PMCD_CALL extern ClientInfo *GetClient(int);
PMCD_CALL extern int SetClientAttribute(int, int, char *);
PMCD_CALL extern void ShowClients(FILE *m);
extern int CheckClientAccess(ClientInfo *);
extern int CheckAccountAccess(ClientInfo *);

extern char *nameclient(int);

#endif /* PMCD_CLIENT_H */
