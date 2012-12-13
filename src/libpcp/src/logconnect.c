/*
 * Copyright (c) 2012 Red Hat.
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
 * Thread-safe notes
 *
 * Do not need ctxp->c_pmcd->pc_lock lock around __pmSendCreds() call,
 * as the connection to pmlogger has not been created, so no-one else
 * could be using the fd.
 */

#include "pmapi.h"
#include "impl.h"

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

/*
 * Return timeout (in seconds) to be used when pmlc is communicating
 * with pmlogger ... used externally from pmlc and internally from
 * __pmConnectLogger() and __pmControlLogger()
 */
int
__pmLoggerTimeout(void)
{
    static int		timeout = TIMEOUT_NEVER;
    static int		done_default = 0;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (!done_default) {
	char	*timeout_str;
	char	*end_ptr;
	if ((timeout_str = getenv("PMLOGGER_REQUEST_TIMEOUT")) != NULL) {
	    /*
	     * Only a positive integer (the unit is seconds) is OK
	     */
	    timeout = strtol(timeout_str, &end_ptr, 10);
	    if (*end_ptr != '\0' || timeout < 0) {
		__pmNotifyErr(LOG_WARNING,
			      "ignored bad PMLOGGER_REQUEST_TIMEOUT = '%s'\n",
			      timeout_str);
		timeout = TIMEOUT_NEVER;
	    }
	}
	done_default = 1;
    }
    PM_UNLOCK(__pmLock_libpcp);

    return timeout;
}

/*
 * expect one of pid or port to be 0 ... if port is 0, use
 * hostname+pid to find port, assuming pmcd is running there
 */
int
__pmConnectLogger(const char *hostname, int *pid, int *port)
{
    int			n, sts;
    __pmLogPort		*lpp;
    struct __pmSockAddr *myAddr;
    struct __pmHostEnt	*servInfo;
    int			fd;	/* Fd for socket connection to pmcd */
    __pmPDU		*pb;
    __pmPDUHdr		*php;
    int			pinpdu;

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
	    if (pmDebug & DBG_TRACE_CONTEXT) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "__pmConnectLogger: __pmLogFindPort: %s\n", pmErrStr_r(n, errmsg, sizeof(errmsg)));
	    }
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

    if ((servInfo = __pmAllocHostEnt()) == NULL) {
	return -ENOMEM;
    }
    if ((myAddr = __pmAllocSockAddr()) == NULL) {
	__pmFreeHostEnt(servInfo);
	return -ENOMEM;
    }

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (__pmGetHostByName(hostname, servInfo) == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: gethostbyname: %s\n",
		    hoststrerror());
#endif
	PM_UNLOCK(__pmLock_libpcp);
	__pmFreeSockAddr(myAddr);
	__pmFreeHostEnt(servInfo);
	return -ECONNREFUSED;
    }

    /* Create socket and attempt to connect to the pmlogger control port */
    if ((fd = __pmCreateSocket()) < 0) {
	PM_UNLOCK(__pmLock_libpcp);
	__pmFreeSockAddr(myAddr);
	__pmFreeHostEnt(servInfo);
	return fd;
    }

    __pmInitSockAddr(myAddr, 0, htons(*port));
    __pmSetSockAddr(myAddr, servInfo);
    PM_UNLOCK(__pmLock_libpcp);

    sts = __pmConnect(fd, myAddr, __pmSockAddrSize());

    __pmFreeSockAddr(myAddr);
    __pmFreeHostEnt(servInfo);

    if (sts < 0) {
	sts = neterror();
	if (sts == EINPROGRESS) {
	  /* We're in progress - wait on select. */
	  struct timeval stv = { 0, 000000 };
	  struct timeval *pstv;
	  __pmFdSet rfds;
	  int rc;
	  stv.tv_sec = __pmLoggerTimeout();
	  pstv = stv.tv_sec ? &stv : NULL;

	  __pmFD_ZERO(&rfds);
	  __pmFD_SET(fd, &rfds);
	  sts = 0;
	  if ((rc = __pmSelectRead(fd+1, &rfds, pstv)) == 1) {
	    sts = __pmConnectCheckError(fd);
	  }
	  else if (rc == 0) {
	    sts = ETIMEDOUT;
	  }
	  else {
	    sts = (rc < 0) ? neterror() : EINVAL;
	  }
	}
	sts = -sts;
	if (sts < 0) {
	  __pmCloseSocket(fd);
#ifdef PCP_DEBUG
	  if (pmDebug & DBG_TRACE_CONTEXT) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmConnectLogger: connect: %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	  }
#endif
	  return sts;
	}
    }

    /* Expect an error PDU back: ACK/NACK for connection */
    pinpdu = sts = __pmGetPDU(fd, ANY_SIZE, __pmLoggerTimeout(), &pb);
    if (sts == PDU_ERROR) {
	__pmOverrideLastFd(PDU_OVERRIDE2);	/* don't dink with the value */
	__pmDecodeError(pb, &sts);
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
    else if (sts < 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    if (sts == PM_ERR_TIMEOUT)
		fprintf(stderr, "__pmConnectLogger: timeout (after %d secs)\n", __pmLoggerTimeout());
	    else {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "__pmConnectLogger: Error: %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
	}
#endif
	;	/* fall through */
    }
    else {
	/* wrong PDU type! */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: ACK botch PDU type=%d not PDU_ERROR?\n", sts);
#endif
	sts = PM_ERR_IPC;
    }

    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);

    if (sts >= 0) {
	if (sts == LOG_PDU_VERSION2) {
	    __pmCred	handshake[1];

	    __pmSetVersionIPC(fd, sts);
	    handshake[0].c_type = CVERSION;
	    handshake[0].c_vala = LOG_PDU_VERSION;
	    handshake[0].c_valb = 0;
	    handshake[0].c_valc = 0;
	    sts = __pmSendCreds(fd, (int)getpid(), 1, handshake);
	}
	else
	    sts = PM_ERR_IPC;
	if (sts >= 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "__pmConnectLogger: PDU version=%d fd=%d\n",
					__pmVersionIPC(fd), fd);
#endif
	    return fd;
	}
    }
    /* error if we get here */
    __pmCloseSocket(fd);
    return sts;
}
