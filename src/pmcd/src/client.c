/*
 * Copyright (c) 2012-2018 Red Hat.
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
 */

#include "pmapi.h"
#include "libpcp.h"
#include "pmcd.h"

#define MIN_CLIENTS_ALLOC 8

int		maxClientFd = -1;	/* largest fd for a client */
__pmFdSet	clientFds;		/* for client select() */

static int	clientSize;

/*
 * For PMDA_INTERFACE_5 or later PMDAs, post a notification that
 * a context has been closed.
 */
static void
NotifyEndContext(int ctx)
{
    int i;

    for (i = 0; i < nAgents; i++) {
	if (!agent[i].status.connected ||
	    agent[i].status.busy || agent[i].status.notReady)
	    continue;
	if (agent[i].ipcType == AGENT_DSO) {
	    pmdaInterface	*dp = &agent[i].ipc.dso.dispatch;
	    if (dp->comm.pmda_interface >= PMDA_INTERFACE_5) {
		if (dp->version.four.ext->e_endCallBack != NULL) {
		    if (pmDebugOptions.context) {
			fprintf(stderr, "NotifyEndContext: DSO PMDA %s (%d) notified of context %d close\n",
			    agent[i].pmDomainLabel, agent[i].pmDomainId,
			    ctx);
		    }
		    (*(dp->version.four.ext->e_endCallBack))(ctx);
		}
	    }
	}
	else {
	    /*
	     * Daemon PMDA case ... we don't know the PMDA_INTERFACE
	     * version, so send the notification PDU anyway, and rely on
	     * __pmdaMainPDU() doing the right thing.
	     * Do not expect a response.
	     * Agent may have decided to spontaneously die so don't
	     * bother about any return status from the __pmSendError
	     * either.
	     */
	    if (pmDebugOptions.context) {
		fprintf(stderr, "NotifyEndContext: daemon PMDA %s (%d) notified of context %d close\n",
		    agent[i].pmDomainLabel, agent[i].pmDomainId, ctx);
	    }
	    pmcd_trace(TR_XMIT_PDU, agent[i].inFd, PDU_ERROR, PM_ERR_NOTCONN);
	    __pmSendError(agent[i].inFd, ctx, PM_ERR_NOTCONN);
	}
    }
}

/* Establish a new socket connection to a client */
ClientInfo *
AcceptNewClient(int reqfd)
{
    static unsigned int	seq, saved, count;
    static struct timeval then;
    int			i, fd;
    __pmSockLen		addrlen;
    struct timeval	now;

    i = NewClient();
    addrlen = __pmSockAddrSize();
    fd = __pmAccept(reqfd, client[i].addr, &addrlen);
    if (fd == -1) {
	if (neterror() == ECONNABORTED) {
	    /* quietly ignore this one ... */
	    ;
	}
	else {
	    /* Permission denied or an unexpected error (e.g. EMFILE)
	     * - rate limit the logging and make this client go away.
	     */
	    pmtimevalNow(&now);
	    if (neterror() != saved || now.tv_sec > then.tv_sec + 60) {
		if (neterror() == EPERM)
		    pmNotifyErr(LOG_NOTICE, "AcceptNewClient(%d): "
				"Permission Denied (%d suppressed)\n",
				reqfd, count);
		else
		    pmNotifyErr(LOG_ERR, "AcceptNewClient(%d): "
				"Accept error (%d suppressed): %d: %s\n",
				reqfd, count, neterror(), netstrerror());
		saved = neterror();
		count = 0;
	    } else {
		count++;
	    }
	    then = now;
	}
	client[i].fd = -1;
	DeleteClient(&client[i]);
	return NULL;	
    }
    if (fd > maxClientFd)
	maxClientFd = fd;

    pmcd_openfds_sethi(fd);

    __pmFD_SET(fd, &clientFds);
    __pmSetVersionIPC(fd, UNKNOWN_VERSION);	/* before negotiation */
    __pmSetSocketIPC(fd);

    client[i].fd = fd;
    client[i].status.connected = 1;
    client[i].status.attributes = 0;
    client[i].status.changes = 0;
    memset(&client[i].attrs, 0, sizeof(__pmHashCtl));

    /*
     * Note seq needs to be unique, but we're using a free running counter
     * and not bothering to check here ... unless we churn through
     * 4,294,967,296 (2^32) clients while one client remains connected
     * we won't have a problem
     */
    client[i].seq = seq++;
    pmtimevalNow(&now);
    client[i].start = now.tv_sec;

    if (pmDebugOptions.appl3)
	fprintf(stderr, "AcceptNewClient(%d): client[%d] (fd %d)\n", reqfd, i, fd);
    pmcd_trace(TR_ADD_CLIENT, i, 0, 0);

    return &client[i];
}

int
NewClient(void)
{
    int i, sz;

    for (i = 0; i < nClients; i++)
	if (!client[i].status.connected)
	    break;

    if (i == clientSize) {
	clientSize = clientSize ? clientSize * 2 : MIN_CLIENTS_ALLOC;
	sz = sizeof(ClientInfo) * clientSize;
	client = (ClientInfo *) realloc(client, sz);
	if (client == NULL) {
	    pmNoMem("NewClient", sz, PM_RECOV_ERR);
	    Shutdown();
	    exit(1);
	}
	sz -= (sizeof(ClientInfo) * i);
	memset(&client[i], 0, sz);
    }
    client[i].addr = __pmSockAddrAlloc();
    if (client[i].addr == NULL) {
        pmNoMem("NewClient", __pmSockAddrSize(), PM_RECOV_ERR);
	Shutdown();
	exit(1);
    }
    if (i >= nClients)
	nClients = i + 1;
    return i;
}

void
DeleteClient(ClientInfo *cp)
{
    __pmHashCtl		*hcp;
    int			i;

    for (i = 0; i < nClients; i++)
	if (cp == &client[i])
	    break;

    if (i == nClients) {
	if (pmDebugOptions.appl3) {
	    pmNotifyErr(LOG_ERR, "DeleteClient: tried to delete non-existent client\n");
	    Shutdown();
	    exit(1);
	}
	return;
    }
    if (cp->fd != -1) {
	__pmFD_CLR(cp->fd, &clientFds);
	__pmCloseSocket(cp->fd);
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
    hcp = &cp->profile;
    for (i = 0; i < hcp->hsize; i++) {
	__pmHashNode	*hp;
	for (hp = hcp->hash[i]; hp != NULL; hp = hp->next) {
	    if (hp->data != NULL)
		__pmFreeProfile((pmProfile *)hp->data);
	}
    }
    __pmHashFree(hcp);
    __pmFreeAttrsSpec(&cp->attrs);
    __pmHashClear(&cp->attrs);
    __pmSockAddrFree(cp->addr);
    cp->addr = NULL;
    cp->status.connected = 0;
    cp->status.attributes = 0;
    cp->status.changes = 0;
    cp->fd = -1;

    NotifyEndContext(cp-client);
}

void
MarkStateChanges(unsigned int changes)
{
    int i;

    for (i = 0; i < nClients; i++) {
	if (client[i].status.connected == 0)
	    continue;
	client[i].status.changes |= changes;
    }
}

int
CheckAccountAccess(ClientInfo *cp)
{
    __pmHashNode *node;
    const char *userid;
    const char *groupid;

    userid = ((node = __pmHashSearch(PCP_ATTR_USERID, &cp->attrs)) ?
		(const char *)node->data : NULL);
    groupid = ((node = __pmHashSearch(PCP_ATTR_GROUPID, &cp->attrs)) ?
		(const char *)node->data : NULL);
    if (pmDebugOptions.auth)
	fprintf(stderr, "CheckAccountAccess: client fd=%d userid=%s groupid=%s\n", cp->fd, userid, groupid);
    if (!userid || !groupid)
	if (__pmServerHasFeature(PM_SERVER_FEATURE_CREDS_REQD))
	    return PM_ERR_PERMISSION;
    return __pmAccAddAccount(userid, groupid, &cp->denyOps);
}

int
CheckClientAccess(ClientInfo *cp)
{
    return __pmAccAddClient(cp->addr, &cp->denyOps);
}
