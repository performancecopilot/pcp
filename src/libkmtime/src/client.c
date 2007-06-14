/*
 * Copyright (c) 2006 Nathan Scott.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "kmtime.h"

static int kmSocketSetup(void)
{
    int	fd, nodelay = 1;
    struct linger nolinger = { 1, 0 };

    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	return -1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, /* avoid 200 ms delay */
		   (char *)&nodelay, (socklen_t)sizeof(nodelay)) < 0) {
        close(fd);
        return -2;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, /* don't linger on close */
		   (char *)&nolinger, (socklen_t)sizeof(nolinger)) < 0) {
        close(fd);
        return -3;
    }
    return fd;
}

static int kmServerExec(int fd, int livemode)
{
    char portname[32];
    int pid, sts, port = -1, pfd[2] = { -1, -1 };

    if (pipe(pfd) < 0)
	goto error;
    if ((pid = fork()) < 0) {
	goto error;
    } else if (pid == 0) {	/* child (kmtime) */
	if (pfd[1] != STDOUT_FILENO) {
	    if (dup2(pfd[1], STDOUT_FILENO) != STDOUT_FILENO) {
		errno = EBADF;
		goto error;
	    }
	    close(pfd[1]);	/* not needed after dup2 */
	}
	close(pfd[0]);	/* close read end */
	execlp("kmtime", "kmtime", livemode ? "-h" : "-a", NULL);
	_exit(127);
    } else {			/* parent (self) */
	if ((sts = read(pfd[0], &portname, sizeof(portname))) < 0)
	    goto error;
	if (sscanf(portname, "port=%d", &port) != 1) {
	    errno = EPROTO;
	    goto error;
	}
	close(pfd[0]);
	close(pfd[1]);
    }
    return port;

error:
    if (pfd[0] != -1)
	close(pfd[0]);
    if (pfd[1] != -1)
	close(pfd[1]);
    close(fd);
    return -1;
}

static int kmConnectHandshake(int fd, int port, kmTime *pkt)
{
    struct sockaddr_in myaddr;
    char buffer[4096];
    kmTime *ack;
    int sts;

    /*
     * Connect to kmtime - kmtime guaranteed started by now, due to the
     * port number read(2) earlier, or -p option (so no race there).
     */
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    myaddr.sin_port = htons(port);
    if ((sts = connect(fd, (struct sockaddr *)&myaddr, sizeof(myaddr))) < 0)
	goto error;

    /*
     * Write the packet, then wait for an ACK.
     */
    sts = write(fd, pkt, pkt->length);
    if (sts < 0) {
	goto error;
    } else if (sts != pkt->length) {
	errno = EMSGSIZE;
	goto error;
    }
    ack = (kmTime *)buffer;
    sts = read(fd, buffer, sizeof(buffer));
    if (sts < 0) {
	goto error;
    } else if (sts != ack->length) {
	errno = EMSGSIZE;
	goto error;
    } else if (ack->command != KM_TCTL_ACK) {
	errno = EPROTO;
	goto error;
    } else if (ack->source != pkt->source) {
	errno = ENOSYS;
	goto error;
    }
    return 0;

error:
    close(fd);
    return -1;
}

int kmTimeConnect(int port, kmTime *pkt)
{
    int	fd;

    if ((fd = kmSocketSetup()) < 0)
	return -1;
    if (port < 0) {
	if ((port = kmServerExec(fd, pkt->source != KM_SOURCE_ARCHIVE)) < 0)
	    return -2;
	if (kmConnectHandshake(fd, port, pkt) < 0)
	    return -3;
    } else {		/* attempt to connect to the given port (once) */
	if (kmConnectHandshake(fd, port, pkt) < 0)
	    return -4;
    }
    return fd;
}

int kmTimeSendAck(int fd, struct timeval *tv)
{
    kmTime data;

    memset(&data, 0, sizeof(data));
    data.magic = KMTIME_MAGIC;
    data.length = sizeof(data);
    data.command = KM_TCTL_ACK;
    data.position = *tv;
    return write(fd, &data, sizeof(data));
}

int kmTimeShowDialog(int fd, int show)
{
    kmTime data;
    int sts;

    memset(&data, 0, sizeof(data));
    data.magic = KMTIME_MAGIC;
    data.length = sizeof(data);
    data.command = show ? KM_TCTL_SHOWDIALOG : KM_TCTL_HIDEDIALOG;
    sts = write(fd, &data, sizeof(data));
    if (sts >= 0 && sts != sizeof(data)) {
	errno = ENODATA;
	sts = -1;
    }
    return sts;
}

int kmTimeRecv(int fd, kmTime **datap)
{
    kmTime *k = *datap;
    int sts, remains;

    memset(k, 0, sizeof(kmTime));
    sts = read(fd, k, sizeof(kmTime));
    if (sts >= 0 && sts != sizeof(kmTime)) {
	errno = ENODATA;
	sts = -1;
    } else if (k->length > sizeof(kmTime)) {	/* double dipping */
	remains = k->length - sizeof(kmTime);
	*datap = k = realloc(k, k->length);
	sts = read(fd, (char *)k + sizeof(kmTime), remains);
	if (sts >= 0 && sts != remains) {
	    errno = E2BIG;
	    sts = -1;
	}
    }
    if (sts < 0)
	return sts;
    return k->command;
}
