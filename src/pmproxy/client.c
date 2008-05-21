/*
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include "pmproxy.h"

#define MIN_CLIENTS_ALLOC 8

ClientInfo	*client;
int		nClients;		/* Number in array, (not all in use) */
int		maxSockFd = -1;		/* largest fd for a client */
fd_set		sockFds;		/* for client select() */

static int	clientSize;

extern void	Shutdown(void);

static int
NewClient(void)
{
    int i;

    for (i = 0; i < nClients; i++)
	if (!client[i].status.connected)
	    break;

    if (i == clientSize) {
	clientSize = clientSize ? clientSize * 2 : MIN_CLIENTS_ALLOC;
	client = (ClientInfo*)
	    realloc(client, sizeof(ClientInfo) * clientSize);
	if (client == NULL) {
	    __pmNoMem("NewClient", sizeof(ClientInfo) * clientSize, PM_RECOV_ERR);
	    Shutdown();
	    exit(1);
	}
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
    mysocklen_t	addrlen;
    int		ok = 0;
    char	buf[MY_BUFLEN];
    char	*bp;
    char	*endp;

    i = NewClient();
    addrlen = sizeof(client[i].addr);
    fd = accept(reqfd, (struct sockaddr *)&client[i].addr, &addrlen);
    if (fd == -1) {
	fprintf(stderr, "AcceptNewClient(%d) accept: %s\n",
	    reqfd, strerror(errno));
	Shutdown();
	exit(1);
    }
    if (fd > maxSockFd)
	maxSockFd = fd;
    FD_SET(fd, &sockFds);

    client[i].fd = fd;
    client[i].status.connected = 1;
    client[i].pmcd_hostname = NULL;

    /*
     * version negotiation (converse to negotiate_proxy() logic in
     * libpcp
     *
     *   recv client version message
     *   send my server version message
     *   recv pmcd hostname and pmcd port
     */
    for (bp = buf; bp < &buf[MY_BUFLEN]; bp++) {
	if (read(fd, bp, 1) != 1) {
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
	fprintf(stderr, "AcceptNewClient: bad version string (");
	for (bp = buf; *bp && bp < &buf[MY_BUFLEN]; bp++)
	    fputc(*bp & 0xff, stderr);
	fprintf(stderr, ") recv from client at %s\n",
	    inet_ntoa(client[i].addr.sin_addr));
	DeleteClient(&client[i]);
	return NULL;
    }

    if (write(fd, MY_VERSION, strlen(MY_VERSION)) != strlen(MY_VERSION)) {
	fprintf(stderr, "AcceptNewClient: failed to send version string (%s) to client at %s\n",
	    MY_VERSION, inet_ntoa(client[i].addr.sin_addr));

	DeleteClient(&client[i]);
	return NULL;
    }

    for (bp = buf; bp < &buf[MY_BUFLEN]; bp++) {
	if (read(fd, bp, 1) != 1) {
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
	*bp = '\0';
	client[i].pmcd_hostname = strdup(buf);
	if (client[i].pmcd_hostname == NULL) {
	    /* this is fatal folks */
	    fprintf(stderr, "AcceptNewClient: PMCD hostname malloc failed: %s\n", pmErrStr(-errno));
	    exit(1);
	}
	bp++;
	client[i].pmcd_port = (int)strtoul(bp, &endp, 10);
	if (*endp != '\0') {
	    /* bad port number */
	    fprintf(stderr, "AcceptNewClient: bad pmcd port \"%s\"", bp);
	    fprintf(stderr, " recv from client at %s\n",
		inet_ntoa(client[i].addr.sin_addr));
	    DeleteClient(&client[i]);
	    return NULL;
	}
    }

    if (client[i].pmcd_hostname == NULL) {
	fprintf(stderr, "AcceptNewClient: failed to get PMCD hostname (%s) from client at %s\n",
	    buf, inet_ntoa(client[i].addr.sin_addr));
	DeleteClient(&client[i]);
	return NULL;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	/*
	 * note error message gets appended to once pmcd connection is
	 * made in ClientLoop()
	 */
	fprintf(stderr, "AcceptNewClient [%d] fd=%d from %s to %s (port %s)",
	    i, fd, inet_ntoa(client[i].addr.sin_addr),
	    client[i].pmcd_hostname, bp);
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

    if (cp->fd != -1) {
	__pmResetIPC(cp->fd);
	FD_CLR(cp->fd, &sockFds);
	close(cp->fd);
    }
    if (cp->pmcd_fd >= 0) {
	close(cp->pmcd_fd);
	FD_CLR(cp->pmcd_fd, &sockFds);
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
    cp->status.connected = 0;
    cp->fd = -1;
    cp->pmcd_fd = -1;
    if (cp->pmcd_hostname != NULL) {
	free(cp->pmcd_hostname);
	cp->pmcd_hostname = NULL;
    }
}
