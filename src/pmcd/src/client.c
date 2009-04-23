/*
 * Copyright (c) 1995-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmcd.h"

#define MIN_CLIENTS_ALLOC 8

int		maxClientFd = -1;	/* largest fd for a client */
fd_set		clientFds;		/* for client select() */

static int	clientSize = 0;

extern void	Shutdown(void);

/* Establish a new socket connection to a client */
ClientInfo *
AcceptNewClient(int reqfd)
{
    static unsigned int	seq = 0;
    int		i, fd;
    mysocklen_t	addrlen;

    i = NewClient();
    addrlen = sizeof(client[i].addr);
    fd = accept(reqfd, (struct sockaddr *)&client[i].addr, &addrlen);
    if (fd == -1) {
    	if (errno == EPERM) {
	    __pmNotifyErr(LOG_NOTICE, "AcceptNewClient(%d): "
	 	          "Permission Denied\n", reqfd);
	    client[i].fd = -1;
	    DeleteClient(&client[i]);
	    return NULL;	
	}
	else {
	    __pmNotifyErr(LOG_ERR, "AcceptNewClient(%d) accept: %s\n",
	    reqfd, strerror(errno));
	    Shutdown();
	    exit(1);
	}
    }
    if (fd > maxClientFd)
	maxClientFd = fd;

    PMCD_OPENFDS_SETHI(fd);

    FD_SET(fd, &clientFds);
    __pmSetVersionIPC(fd, UNKNOWN_VERSION);	/* before negotiation */
    __pmSetSocketIPC(fd);
    client[i].fd = fd;
    client[i].status.connected = 1;
    client[i].status.changes = 0;
    /*
     * Note seq needs to be unique, but we're using a free running counter
     * and not bothering to check here ... unless we churn through
     * 4,294,967,296 (2^32) clients while one client remains connected
     * we won't have a problem
     */
    client[i].seq = seq++;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "AcceptNewClient(%d): client[%d] (fd %d)\n", reqfd, i, fd);
#endif
    pmcd_trace(TR_ADD_CLIENT, client[i].addr.sin_addr.s_addr, fd, client[i].seq);

    return &client[i];
}

int
NewClient(void)
{
    int i;

    for (i = 0; i < nClients; i++)
	if (!client[i].status.connected)
	    break;

    if (i == clientSize) {
	int	j;
	clientSize = clientSize ? clientSize * 2 : MIN_CLIENTS_ALLOC;
	client = (ClientInfo*)
	    realloc(client, sizeof(ClientInfo) * clientSize);
	if (client == NULL) {
	    __pmNoMem("NewClient", sizeof(ClientInfo) * clientSize, PM_RECOV_ERR);
	    Shutdown();
	    exit(1);
	}
	for (j = i; j < clientSize; j++) {
	    client[j].profile = NULL;
	    client[j].szProfile = 0;
	}
    }
    if (i >= nClients)
	nClients = i + 1;
    return i;
}

void
DeleteClient(ClientInfo *cp)
{
    int		i;

    for (i = 0; i < nClients; i++)
	if (cp == &client[i])
	    break;

    if (i == nClients) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    __pmNotifyErr(LOG_ERR, "DeleteClient: tried to delete non-existent client\n");
	    Shutdown();
	    exit(1);
	}
#endif
	return;
    }
    if (cp->fd != -1) {
	__pmResetIPC(cp->fd);
	FD_CLR(cp->fd, &clientFds);
	close(cp->fd);
    }
    if (i == nClients-1) {
	i--;
	while (i >= 0 && !client[i].status.connected)
	    i--;
	nClients = (i >= 0) ? i + 1 : 0;
    }
    if (cp->fd == maxClientFd) {
	maxClientFd = -1;
	for (i = 0; i < nClients; i++) {
	    if (client[i].fd > maxClientFd)
		maxClientFd = client[i].fd;
	}
    }
    for (i = 0; i < cp->szProfile; i++) {
	if (cp->profile[i] != NULL) {
	    __pmFreeProfile(cp->profile[i]);
	    cp->profile[i] = NULL;
	}
    }
    cp->status.connected = 0;
    cp->fd = -1;
}

void
MarkStateChanges(int changes)
{
    int			i;

    for (i = 0; i < nClients; i++) {
	if (client[i].status.connected == 0)
	    continue;
	client[i].status.changes |= changes;
    }
}
