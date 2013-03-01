/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmproxy.h"

#define MIN_CLIENTS_ALLOC 8

ClientInfo	*client;
int		nClients;		/* Number in array, (not all in use) */
int		maxReqPortFd;		/* highest request port fd */
int		maxSockFd;		/* largest fd for a client */
__pmFdSet	sockFds;		/* for client select() */

static int
NewClient(void)
{
    int		i;
    static int	clientSize;

    for (i = 0; i < nClients; i++)
	if (!client[i].status.connected)
	    break;

    if (i == clientSize) {
	int j, sz;

	clientSize = clientSize ? clientSize * 2 : MIN_CLIENTS_ALLOC;
	sz = sizeof(ClientInfo) * clientSize;
	client = (ClientInfo *) realloc(client, sz);
	if (client == NULL) {
	    __pmNoMem("NewClient", sz, PM_RECOV_ERR);
	    Shutdown();
	    exit(1);
	}
	for (j = i; j < clientSize; j++) {
	    client[j].addr = NULL;
	}
    }
    client[i].addr = __pmSockAddrAlloc();
    if (client[i].addr == NULL) {
        __pmNoMem("NewClient", __pmSockAddrSize(), PM_RECOV_ERR);
	Shutdown();
	exit(1);
    }
    if (i >= nClients)
	nClients = i + 1;
    return i;
}

/* MY_BUFLEN needs to big enough to hold "hostname port" */
#define MY_BUFLEN (MAXHOSTNAMELEN+10)
#define MY_VERSION "pmproxy-server 1\n"

/* Establish a new socket connection to a client */
ClientInfo *
AcceptNewClient(int reqfd)
{
    int		i;
    int		fd;
    __pmSockLen	addrlen;
    int		ok = 0;
    char	buf[MY_BUFLEN];
    char	*bp;
    char	*endp;
    char	*abufp;

    i = NewClient();
    addrlen = __pmSockAddrSize();
    fd = __pmAccept(reqfd, client[i].addr, &addrlen);
    if (fd == -1) {
	__pmNotifyErr(LOG_ERR, "AcceptNewClient(%d) __pmAccept failed: %s",
			reqfd, netstrerror());
	Shutdown();
	exit(1);
    }
    __pmSetSocketIPC(fd);
    if (fd > maxSockFd)
	maxSockFd = fd;
    __pmFD_SET(fd, &sockFds);

    client[i].fd = fd;
    client[i].pmcd_fd = -1;
    client[i].status.connected = 1;
    client[i].pmcd_hostname = NULL;

    /*
     * version negotiation (converse to negotiate_proxy() logic in
     * libpcp
     *
     *   __pmRecv client version message
     *   __pmSend my server version message
     *   __pmRecv pmcd hostname and pmcd port
     */
    for (bp = buf; bp < &buf[MY_BUFLEN]; bp++) {
	if (__pmRecv(fd, bp, 1, 0) != 1) {
	    *bp = '\0';		/* null terminate what we have */
	    bp = &buf[MY_BUFLEN];	/* flag error */
	    break;
	}
	/* end of line means no more ... */
	if (*bp == '\n' || *bp == '\r') {
	    *bp = '\0';
	    break;
	}
    }
    if (bp < &buf[MY_BUFLEN]) {
	/* looks OK so far ... is this a version we can support? */
	if (strcmp(buf, "pmproxy-client 1") == 0) {
	    client[i].version = 1;
	    ok = 1;
	}
    }

    if (!ok) {
	abufp = __pmSockAddrToString(client[i].addr);
	__pmNotifyErr(LOG_WARNING, "Bad version string from client at %s", abufp);
	free(abufp);
	fprintf(stderr, "AcceptNewClient: bad version string was \"");
	for (bp = buf; *bp && bp < &buf[MY_BUFLEN]; bp++)
	    fputc(*bp & 0xff, stderr);
	fprintf(stderr, "\"\n");
	DeleteClient(&client[i]);
	return NULL;
    }

    if (__pmSend(fd, MY_VERSION, strlen(MY_VERSION), 0) != strlen(MY_VERSION)) {
	abufp = __pmSockAddrToString(client[i].addr);
	__pmNotifyErr(LOG_WARNING, "AcceptNewClient: failed to send version "
			"string (%s) to client at %s\n", MY_VERSION, abufp);
	free(abufp);
	DeleteClient(&client[i]);
	return NULL;
    }

    for (bp = buf; bp < &buf[MY_BUFLEN]; bp++) {
	if (__pmRecv(fd, bp, 1, 0) != 1) {
	    *bp = '\0';		/* null terminate what we have */
	    bp = &buf[MY_BUFLEN];	/* flag error */
	    break;
	}
	/* end of line means no more ... */
	if (*bp == '\n' || *bp == '\r') {
	    *bp = '\0';
	    break;
	}
    }
    if (bp < &buf[MY_BUFLEN]) {
	/* looks OK so far ... get hostname and port */
	for (bp = buf; *bp && *bp != ' '; bp++)
	    ;
	if (bp != buf) {
	    *bp = '\0';
	    client[i].pmcd_hostname = strdup(buf);
	    if (client[i].pmcd_hostname == NULL)
		__pmNoMem("PMCD.hostname", strlen(buf), PM_FATAL_ERR);
	    bp++;
	    client[i].pmcd_port = (int)strtoul(bp, &endp, 10);
	    if (*endp != '\0') {
		abufp = __pmSockAddrToString(client[i].addr);
		__pmNotifyErr(LOG_WARNING, "AcceptNewClient: bad pmcd port "
				"\"%s\" from client at %s", bp, abufp);
		free(abufp);
		DeleteClient(&client[i]);
		return NULL;
	    }
	}
	/* error, fall through */
    }

    if (client[i].pmcd_hostname == NULL) {
	abufp = __pmSockAddrToString(client[i].addr);
	__pmNotifyErr(LOG_WARNING, "AcceptNewClient: failed to get PMCD "
				"hostname (%s) from client at %s", buf, abufp);
	free(abufp);
	DeleteClient(&client[i]);
	return NULL;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	/*
	 * note error message gets appended to once pmcd connection is
	 * made in ClientLoop()
	 */
	abufp = __pmSockAddrToString(client[i].addr);
	fprintf(stderr, "AcceptNewClient [%d] fd=%d from %s to %s (port %s)",
		i, fd, abufp, client[i].pmcd_hostname, bp);
	free(abufp);
    }
#endif

    return &client[i];
}

void
DeleteClient(ClientInfo *cp)
{
    int		i;

    for (i = 0; i < nClients; i++)
	if (cp == &client[i])
	    break;

    if (i == nClients) {
	fprintf(stderr, "DeleteClient: Botch: tried to delete non-existent client @" PRINTF_P_PFX "%p\n", cp);
	return;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "DeleteClient [%d]\n", i);
#endif

    if (cp->fd >= 0) {
	__pmFD_CLR(cp->fd, &sockFds);
	__pmCloseSocket(cp->fd);
    }
    if (cp->pmcd_fd >= 0) {
	__pmFD_CLR(cp->pmcd_fd, &sockFds);
	__pmCloseSocket(cp->pmcd_fd);
    }
    if (i == nClients-1) {
	i--;
	while (i >= 0 && !client[i].status.connected)
	    i--;
	nClients = (i >= 0) ? i + 1 : 0;
    }
    if (cp->fd == maxSockFd || cp->pmcd_fd == maxSockFd) {
	maxSockFd = maxReqPortFd;
	for (i = 0; i < nClients; i++) {
	    if (cp->status.connected == 0)
		continue;
	    if (client[i].fd > maxSockFd)
		maxSockFd = client[i].fd;
	    if (client[i].pmcd_fd > maxSockFd)
		maxSockFd = client[i].pmcd_fd;
	}
    }
    __pmSockAddrFree(cp->addr);
    cp->addr = NULL;
    cp->status.connected = 0;
    cp->fd = -1;
    cp->pmcd_fd = -1;
    if (cp->pmcd_hostname != NULL) {
	free(cp->pmcd_hostname);
	cp->pmcd_hostname = NULL;
    }
}
