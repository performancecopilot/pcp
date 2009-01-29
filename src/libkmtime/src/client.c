/*
 * Copyright (c) 2006 Aconex.  All Rights Reserved.
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
#include "pmapi.h"
#include "impl.h"
#include "kmtime.h"

static int kmServerExec(int fd, int livemode)
{
    char portname[32];
    int port, in, out;
    char *argv[] = { "kmtime", NULL, NULL };

    if (livemode)
	argv[1] = "-h";	/* -h for live hosts */
    else
	argv[1] = "-a";	/* -a for archives */

    if (__pmProcessCreate(argv, &in, &out) == (pid_t)-1) {
	__pmCloseSocket(fd);
	return -1;
    }

    if (read(out, &portname, sizeof(portname)) < 0)
	port = -1;
    else if (sscanf(portname, "port=%d", &port) != 1) {
	errno = EPROTO;
	port = -1;
    }
    close(in);
    close(out);
    if (port == -1)
	__pmCloseSocket(fd);
    return port;
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
    sts = send(fd, (const void *)pkt, pkt->length, 0);
    if (sts < 0) {
	goto error;
    } else if (sts != pkt->length) {
	errno = EMSGSIZE;
	goto error;
    }
    ack = (kmTime *)buffer;
    sts = recv(fd, buffer, sizeof(buffer), 0);
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
    __pmCloseSocket(fd);
    return -1;
}

int kmTimeConnect(int port, kmTime *pkt)
{
    int	fd;

    if ((fd = __pmCreateSocket()) < 0)
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
    return send(fd, (const void *)&data, sizeof(data), 0);
}

int kmTimeShowDialog(int fd, int show)
{
    kmTime data;
    int sts;

    memset(&data, 0, sizeof(data));
    data.magic = KMTIME_MAGIC;
    data.length = sizeof(data);
    data.command = show ? KM_TCTL_GUISHOW : KM_TCTL_GUIHIDE;
    sts = send(fd, (const void *)&data, sizeof(data), 0);
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
    sts = recv(fd, (void *)k, sizeof(kmTime), 0);
    if (sts >= 0 && sts != sizeof(kmTime)) {
	errno = ENODATA;
	sts = -1;
    } else if (k->length > sizeof(kmTime)) {	/* double dipping */
	remains = k->length - sizeof(kmTime);
	*datap = k = realloc(k, k->length);
	sts = recv(fd, (char *)k + sizeof(kmTime), remains, 0);
	if (sts >= 0 && sts != remains) {
	    errno = E2BIG;
	    sts = -1;
	}
    }
    if (sts < 0)
	return sts;
    return k->command;
}
