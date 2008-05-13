/*
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include "pmapi.h"
#include "impl.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <syslog.h>
#include <netdb.h>
#include <errno.h>
#include <limits.h>

/*
 * expect one of pid or port to be 0 ... if port is 0, use
 * hostname+pid to find port, assuming pmcd is running there
 */
int
__pmConnectLogger(const char *hostname, int *pid, int *port)
{
    int			n, sts;
    __pmLogPort		*lpp;
    struct sockaddr_in	myAddr;
    struct hostent*	servInfo;
    int			fd;	/* Fd for socket connection to pmcd */
    int			nodelay=1;
    struct linger	nolinger = {1, 0};
    __pmPDU		*pb;
    __pmIPC		ipc = { UNKNOWN_VERSION, NULL };
    __pmPDUHdr		*php;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmConnectLogger(host=%s, pid=%d, port=%d)\n",
		hostname, *pid, *port);
#endif

    /*
     * catch pid == PM_LOG_ALL_PIDS ... this tells __pmLogFindPort
     * to get all ports
     */
    if (*pid == PM_LOG_ALL_PIDS) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: pid == PM_LOG_ALL_PIDS makes no sense here\n");
#endif
	return -ECONNREFUSED;
    }

    if (*pid == PM_LOG_NO_PID && *port == PM_LOG_PRIMARY_PORT) {
	/*
	 * __pmLogFindPort can only lookup based on pid, so xlate
	 * the request
	 */
	*pid = PM_LOG_PRIMARY_PID;
	*port = PM_LOG_NO_PORT;
    }

    if (*port == PM_LOG_NO_PORT) {
	if ((n = __pmLogFindPort(hostname, *pid, &lpp)) < 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "__pmConnectLogger: __pmLogFindPort: %s\n", pmErrStr(n));
#endif
	    return n;
	}
	else if (n != 1) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "__pmConnectLogger: __pmLogFindPort -> 1, cannot contact pmcd\n");
#endif
	    return -ECONNREFUSED;
	}
	*port = lpp->port;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: __pmLogFindPort -> pid = %d\n", lpp->port);
#endif
    }

    if ((servInfo = gethostbyname(hostname)) == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: gethostbyname: %s\n",
		    hstrerror(h_errno));
#endif
	return -ECONNREFUSED;
    }

    /* Create socket and attempt to connect to the pmlogger control port */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
	return -errno;

    /* avoid 200 ms delay */
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
		   (char *) &nodelay, (mysocklen_t)sizeof(nodelay)) < 0) {
	sts = -errno;
	close(fd);
	return sts;
    }

    /* don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER,
		   (char *) &nolinger, (mysocklen_t)sizeof(nolinger)) < 0) {
	sts = -errno;
	close(fd);
	return sts;
    }

    memset(&myAddr, 0, sizeof(myAddr));	/* Arrgh! &myAddr, not myAddr */
    myAddr.sin_family = AF_INET;
    memcpy(&myAddr.sin_addr, servInfo->h_addr, servInfo->h_length);
    myAddr.sin_port = htons(*port);

    sts = connect(fd, (struct sockaddr*) &myAddr, sizeof(myAddr));
    if (sts < 0) {
	sts = -errno;
	close(fd);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: connect: %s\n", pmErrStr(sts));
#endif
	return sts;
    }

    /* Expect an error PDU back: ACK/NACK for connection */
    sts = __pmGetPDU(fd, PDU_BINARY, TIMEOUT_NEVER, &pb);
    if (sts == PDU_ERROR) {
	__pmOverrideLastFd(PDU_OVERRIDE2);	/* don't dink with the value */
	__pmDecodeError(pb, PDU_BINARY, &sts);
	if (sts == 0)
	    sts = LOG_PDU_VERSION1;
	else if (sts == PM_ERR_V1(PM_ERR_CONNLIMIT) ||
	         sts == PM_ERR_V1(PM_ERR_PERMISSION)) {
	    /*
	     * we do expect PM_ERR_CONNLIMIT and PM_ERR_PERMISSION as
	     * real responses, even from a PCP 1.x pmcd
	     */
	    sts = XLATE_ERR_1TO2(sts);
	}
	php = (__pmPDUHdr *)pb;
	if (*pid != PM_LOG_NO_PID && *pid != PM_LOG_PRIMARY_PID && php->from != *pid) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "__pmConnectLogger: ACK response from pid %d, expected pid %d\n",
			    php->from, *pid);
#endif
	    sts = -ECONNREFUSED;
	}
	*pid = php->from;
    }
    else {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: ACK PDU type=%d?\n", sts);
#endif
	sts = PM_ERR_IPC;
    }

    if (sts >= 0) {
	if (sts == LOG_PDU_VERSION1) {
#ifdef HAVE_V1_SUPPORT
	    ipc.version = sts;
	    sts = __pmAddIPC(fd, ipc);
#else
	    /* no support for LOG_PDU_VERSION1 any more */
	    pmprintf("__pmConnectLogger: pmlogger PDU version %d not supported\n", ipc.version == 0 ? LOG_PDU_VERSION1 : ipc.version);
	    pmflush();
	    sts = PM_ERR_GENERIC;
#endif
	}
	else if (sts >= LOG_PDU_VERSION2) {
	    __pmCred	handshake[1];
	    ipc.version = sts;
	    sts = __pmAddIPC(fd, ipc);
	    handshake[0].c_type = CVERSION;
	    handshake[0].c_vala = LOG_PDU_VERSION;
	    handshake[0].c_valb = 0;
	    handshake[0].c_valc = 0;
	    sts = __pmSendCreds(fd, PDU_BINARY, 1, handshake);
	}
	if (sts >= 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "__pmConnectLogger: PDU version=%d fd=%d\n",
					ipc.version, fd);
#endif
	    return fd;
	}
    }
    /* error if we get here */
    close(fd);
    return sts;
}
