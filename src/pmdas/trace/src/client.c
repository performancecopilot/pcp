/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2012 Red Hat.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "trace_dev.h"
#include "client.h"
#include "comms.h"

extern __pmFdSet	fds;

int		nclients;		/* number of entries in array */
__pmFD		maxfd;			/* largest fd currently in use */
client_t	*clients;		/* array of clients */

#define MIN_CLIENTS_ALLOC 8
static int	clientsize;

static int newClient(void);

client_t *
acceptClient(int reqfd)
{
    int		i;
    __pmFD	fd;
    mysocklen_t	addrlen;

    i = newClient();
    addrlen = sizeof(clients[i].addr);
    fd = __pmAccept(reqfd, (__pmSockAddr *)&clients[i].addr, &addrlen);
    if (fd == PM_ERROR_FD) {
	__pmNotifyErr(LOG_ERR, "acceptClient(%d) accept: %s",
		reqfd, netstrerror());
	return NULL;
    }
    maxfd = __pmUpdateMaxFD(fd, maxfd);
    __pmFD_SET(fd, &fds);
    clients[i].fd = fd;
    clients[i].status.connected = 1;
    clients[i].status.padding = 0;
    clients[i].status.protocol = 1;	/* sync */
    return &clients[i];
}

static int
newClient(void)
{
    int	i;

    for (i = 0; i < nclients; i++)
	if (!clients[i].status.connected)
	    break;

    if (i == clientsize) {
	clientsize = clientsize ? clientsize * 2 : MIN_CLIENTS_ALLOC;
	clients = (client_t *) realloc(clients, sizeof(client_t)*clientsize);
	if (clients == NULL) {
	    __pmNoMem("newClient", sizeof(client_t)*clientsize, PM_RECOV_ERR);
	    exit(1);
	}
    }
    if (i >= nclients)
	nclients = i + 1;
    return i;
}

void
deleteClient(client_t *cp)
{
    int	i;

    for (i = 0; i < nclients; i++)
	if (cp == &clients[i])
	    break;

    if (i == nclients) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    __pmNotifyErr(LOG_ERR, "deleteClient: tried to delete non-existent client");
	}
#endif
	return;
    }
    if (cp->fd != PM_ERROR_FD) {
	__pmtracenomoreinput(cp->fd);
	__pmFD_CLR(cp->fd, &fds);
	__pmCloseSocket(cp->fd);
    }
    if (cp->fd == maxfd) {
	maxfd = PM_ERROR_FD;
	for (i = 0; i < nclients; i++)
	    maxfd = __pmUpdateMaxFD(clients[i].fd, maxfd);
    }
    cp->status.connected = 0;
    cp->status.padding = 0;
    cp->status.protocol = 1;	/* sync */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    __pmNotifyErr(LOG_DEBUG, "deleteClient: client removed (fd=%d)", cp->fd);
#endif
    cp->fd = PM_ERROR_FD;
}

void
showClients(void)
{
    __pmHostEnt		*hp;
    int			i;

    fprintf(stderr, "%s: %d connected clients:\n", pmProgname, nclients);
    fprintf(stderr, "     fd  type   conn  client connection from\n"
		    "     ==  =====  ====  ======================\n");
    for (i=0; i < nclients; i++) {
	fprintf(stderr, "    %3d", clients[i].fd);
	fprintf(stderr, "  %s  ", clients[i].status.protocol == 1 ? "sync ":"async");
	fprintf(stderr, "%s  ", clients[i].status.connected == 1 ? "up  ":"down");
	hp = __pmGetHostByAddr(& clients[i].addr);
	if (hp == NULL) {
	    char	*p = (char *)&clients[i].addr.sin_addr.s_addr;
	    int		k;

	    for (k=0; k < 4; k++) {
		if (k > 0)
		    fputc('.', stderr);
		fprintf(stderr, "%d", p[k] & 0xff);
	    }
	}
	else
	    fprintf(stderr, "%-40.40s", hp->h_name);
	if (clients[i].denyOps != 0) {
	    fprintf(stderr, "  ");
	    if (clients[i].denyOps & TR_OP_SEND)
		fprintf(stderr, "send ");
        }

	fputc('\n', stderr);
    }
    fputc('\n', stderr);
}
