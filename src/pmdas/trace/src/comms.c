/*
 * Copyright (c) 2012-2013 Red Hat.  All Rights Reserved.
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "pmda.h"
#include "trace.h"
#include "trace_dev.h"
#include "domain.h"
#include "client.h"
#include "comms.h"

extern struct timeval	interval;
extern int readData(int, int *);
extern void timerUpdate(void);

extern int	maxfd;
extern int	nclients;
extern client_t	*clients;
extern int	ctlport;		/* control port number */
static int	ctlfd;			/* fd for control port */
static int	pmcdfd;			/* fd for pmcd */

void alarming(int, void *);
static void hangup(int);
static int getcport(void);

/* currently in-use fd mask */
fd_set	fds;

/* the AF event number */
int	afid = -1;

void
traceMain(pmdaInterface *dispatch)
{
    client_t	*cp;
    fd_set	readyfds;
    int		nready, i, pdutype, sts, protocol;

    ctlfd = getcport();
    pmcdfd = __pmdaInFd(dispatch);
    maxfd = (ctlfd > pmcdfd) ? (ctlfd):(pmcdfd);
    FD_ZERO(&fds);
    FD_SET(ctlfd, &fds);
    FD_SET(pmcdfd, &fds);

    signal(SIGHUP, hangup);

    /* arm interval timer */
    if ((afid = __pmAFregister(&interval, NULL, alarming)) < 0) {
	pmNotifyErr(LOG_ERR, "error registering asynchronous event handler");
	exit(1);
    }

    for (;;) {
	memcpy(&readyfds, &fds, sizeof(readyfds));
	nready = select(maxfd+1, &readyfds, NULL, NULL, NULL);

	if (nready == 0)
	    continue;
	else if (nready < 0) {
	    if (neterror() != EINTR) {
		pmNotifyErr(LOG_ERR, "select failure: %s", netstrerror());
		exit(1);
	    }
	    continue;
	}

	__pmAFblock();
	if (FD_ISSET(pmcdfd, &readyfds)) {
	    if (pmDebugOptions.appl0)
		pmNotifyErr(LOG_DEBUG, "processing pmcd request [fd=%d]", pmcdfd);
	    if (__pmdaMainPDU(dispatch) < 0) {
		__pmAFunblock();
		exit(1);	/* fatal if we lose pmcd */
	    }
	}
	/* handle request on control port */
	if (FD_ISSET(ctlfd, &readyfds)) {
	    if ((cp = acceptClient(ctlfd)) != NULL) {
		sts = __pmAccAddClient(cp->addr, &cp->denyOps);
		if (sts == PM_ERR_PERMISSION)
		    sts = PMTRACE_ERR_PERMISSION;
		else if (sts == PM_ERR_CONNLIMIT)
		    sts = PMTRACE_ERR_CONNLIMIT;
		else if (sts >= 0)
		    sts = TRACE_PDU_VERSION;
		__pmtracesendack(cp->fd, sts);
		if (sts < 0) {
		    if (pmDebugOptions.appl0) {
			char *hostAddr = __pmSockAddrToString(cp->addr);
			pmNotifyErr(LOG_DEBUG, "client %s [fd=%d]: connect refused, denyOps=0x%x: %s",
				 hostAddr, cp->fd, cp->denyOps, pmtraceerrstr(sts));
			free(hostAddr);
		    }
		    deleteClient(cp);
		}
		else {
		    if (pmDebugOptions.appl0) {
			char *hostAddr = __pmSockAddrToString(cp->addr);
			pmNotifyErr(LOG_DEBUG, "client %s [fd=%d]: new connection, denyOps=0x%x",
				 hostAddr, cp->fd, cp->denyOps);
			free(hostAddr);
		    }
		    ;
		}
	    }
	}
	for (i = 0; i < nclients; i++) {
	    if (!clients[i].status.connected)
		continue;
	    if (clients[i].denyOps & TR_OP_SEND) {
		if (pmDebugOptions.appl0) {
		    char *hostAddr = __pmSockAddrToString(clients[i].addr);
		    pmNotifyErr(LOG_DEBUG, "client %s [fd=%d]: send denied, denyOps=0x%x",
			    hostAddr, clients[i].fd, clients[i].denyOps);
		    free(hostAddr);
		}
		__pmtracesendack(clients[i].fd, PMTRACE_ERR_PERMISSION);
		__pmAccDelClient(clients[i].addr);
		deleteClient(&clients[i]);
	    }
	    else if (FD_ISSET(clients[i].fd, &readyfds)) {
		protocol = 1;	/* default to synchronous */
		do {
		    if ((pdutype = readData(clients[i].fd, &protocol)) < 0) {
			if (pmDebugOptions.appl0) {
			    char *hostAddr = __pmSockAddrToString(clients[i].addr);
			    pmNotifyErr(LOG_DEBUG, "client %s [fd=%d]: close connection",
				hostAddr, clients[i].fd);
			    free(hostAddr);
			}
			__pmAccDelClient(clients[i].addr);
			deleteClient(&clients[i]);
		    }
		    else {
			clients[i].status.protocol = protocol;
			if (clients[i].status.connected) {
			    if (pmDebugOptions.appl0) {
				char *hostAddr = __pmSockAddrToString(clients[i].addr);
				pmNotifyErr(LOG_DEBUG, "client %s [fd=%d]: %s ACK (type=%d)",
				    hostAddr, clients[i].fd,
				    protocol ? "sending" : "no", pdutype);
				free(hostAddr);
			    }
			    if (protocol == 1) {
				sts = __pmtracesendack(clients[i].fd, pdutype);
				if (sts < 0) {
				    char *hostAddr = __pmSockAddrToString(clients[i].addr);
				    pmNotifyErr(LOG_ERR, "client %s [fd=%d]: ACK send failed (type=%d): %s",
				        hostAddr, clients[i].fd,
					pdutype, pmtraceerrstr(sts));
				    free(hostAddr);
				}
			    }
			}
		    }
		} while (__pmtracemoreinput(clients[i].fd));
	    }
	}
	__pmAFunblock();
    }
}


void
alarming(int sig, void *ptr)
{
    timerUpdate();
}

static void
hangup(int sig)
{
    showClients();
    signal(SIGHUP, hangup);
}

/*
 * Create socket for incoming connections and bind to it an address for
 * clients to use.  Only returns if it succeeds (exits on failure).
 */
static int
getcport(void)
{
    int			fd;
    int			i=1, one=1, sts;
    struct sockaddr_in  myAddr;
    struct linger	noLinger = {1, 0};

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	pmNotifyErr(LOG_ERR, "getcport: socket: %s", netstrerror());
	exit(1);
    }
    /* avoid 200 ms delay */
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &i,
				    (__pmSockLen)sizeof(i)) < 0) {
	pmNotifyErr(LOG_ERR, "getcport: setsockopt(nodelay): %s",
		netstrerror());
	exit(1);
    }
    /* don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &noLinger,
				    (__pmSockLen)sizeof(noLinger)) < 0) {
	pmNotifyErr(LOG_ERR, "getcport: setsockopt(nolinger): %s",
		netstrerror());
	exit(1);
    }
#ifndef IS_MINGW
    /* ignore dead client connections */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &one,
				    (__pmSockLen)sizeof(one)) < 0) {
	pmNotifyErr(LOG_ERR, "getcport: setsockopt(reuseaddr): %s",
		netstrerror());
	exit(1);
    }
#else
    /* see MSDN tech note: "Using SO_REUSEADDR and SO_EXCLUSIVEADDRUSE" */
    if (setsockopt(sfd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *) &one,
				    (__pmSockLen)sizeof(one)) < 0) {
	pmNotifyErr(LOG_ERR, "getcport: setsockopt(excladdruse): %s",
		netstrerror());
	exit(1);
    }
#endif

    if (ctlport == -1) {
	/*
	 * check for port info in the environment
	 */
	char	*env_str;
	if ((env_str = getenv(TRACE_ENV_PORT)) != NULL) {
	    char	*end_ptr;

	    ctlport = (int)strtol(env_str, &end_ptr, 0);
	    if (*end_ptr != '\0' || ctlport < 0) {
		pmNotifyErr(LOG_WARNING, "env port is bogus (%s)", env_str);
		ctlport = TRACE_PORT;
	    }
	}
	else
	    ctlport = TRACE_PORT;
    }

    /* TODO: IPv6 */
    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myAddr.sin_port = htons(ctlport);

    sts = bind(fd, (const struct sockaddr *)&myAddr, sizeof(myAddr));
    if (sts < 0) {
	pmNotifyErr(LOG_ERR, "bind(%d): %s", ctlport, netstrerror());
	exit(1);
    }
    sts = listen(fd, 5);	/* Max. of 5 pending connection requests */
    if (sts == -1) {
	pmNotifyErr(LOG_ERR, "listen: %s", netstrerror());
	exit(1);
    }

    return fd;
}
