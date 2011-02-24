/*
 * Copyright (c) 2000,2004,2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <fcntl.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

/* default connect timeout is 5 seconds */
static struct timeval	canwait = { 5, 000000 };

int
__pmCreateSocket(void)
{
    int			fd;
    int			sts;
    int			nodelay=1;
    struct linger	nolinger = {1, 0};

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -errno;

    if ((sts = __pmSetSocketIPC(fd)) < 0) {
	__pmCloseSocket(fd);
	return sts;
    }

    /* avoid 200 ms delay */
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay,
		   (mysocklen_t)sizeof(nodelay)) < 0) {
	__pmNotifyErr(LOG_ERR, 
		      "__pmCreateSocket(%d): setsockopt TCP_NODELAY: %s\n",
		      fd, strerror(errno));
    }

    /* don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *)&nolinger,
		   (mysocklen_t)sizeof(nolinger)) < 0) {
	__pmNotifyErr(LOG_ERR, 
		      "__pmCreateSocket(%d): setsockopt SO_LINGER: %s\n",
		      fd, strerror(errno));
    }

    return fd;
}

void
__pmCloseSocket(int fd)
{
    __pmResetIPC(fd);

#if defined(IS_MINGW)
    closesocket(fd);
#else
    close(fd);
#endif
}

int
__pmConnectTo(int fd, const struct sockaddr *addr, int port)
{
    int fdFlags = fcntl(fd, F_GETFL);
    struct sockaddr_in myAddr;

    memcpy(&myAddr, addr, sizeof (struct sockaddr_in));
    myAddr.sin_port = htons(port);

    if (fcntl (fd, F_SETFL, fdFlags | FNDELAY) < 0) {
        __pmNotifyErr(LOG_ERR, "__pmConnectTo: cannot set FNDELAY - "
		      "fcntl(%d,F_SETFL,0x%x) failed: %s\n",
		      fd, fdFlags|FNDELAY , strerror(errno));
    }
    
    if (connect(fd, (struct sockaddr*)&myAddr, sizeof(myAddr)) < 0) {
	if (errno != EINPROGRESS) {
	    close (fd);
	    return -errno;
	}
    }

    return (fdFlags);
}

int
__pmConnectCheckError (int fd)
{
    int	so_err;
    mysocklen_t	olen = sizeof(int);

    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&so_err, &olen) < 0) {
	so_err = errno;
	__pmNotifyErr(LOG_ERR, 
		      "__pmConnectCheckError: getsockopt(SO_ERROR) failed: %s\n",
		      strerror(so_err));

    }

    return (so_err);
}

int
__pmConnectRestoreFlags (int fd, int fdFlags)
{
    int sts;

    if (fcntl(fd, F_SETFL, fdFlags) < 0) {
	__pmNotifyErr(LOG_WARNING,"__pmConnectRestoreFlags: cannot restore "
		      "flags fcntl(%d,F_SETFL,0x%x) failed: %s\n",
		      fd, fdFlags, strerror(errno));
    }

    if ((fdFlags = fcntl(fd, F_GETFD)) >= 0)
        sts = fcntl(fd, F_SETFD, fdFlags | FD_CLOEXEC);
    else
        sts = fdFlags;

    if (sts == -1) {
        __pmNotifyErr(LOG_WARNING, "__pmConnectRestoreFlags: "
		      "fcntl(%d) get/set flags failed: %s\n",
		      fd, strerror(errno));
	close(fd);
	return sts;
    }

    return (fd);
}

const struct timeval *
__pmConnectTimeout(void)
{
    static int		first_time = 1;

    /*
     * get optional stuff from environment ...
     * 	PMCD_CONNECT_TIMEOUT
     *	PMCD_PORT
     */
    if (first_time) {
	char	*env_str;
	first_time = 0;

	if ((env_str = getenv("PMCD_CONNECT_TIMEOUT")) != NULL) {
	    char	*end_ptr;
	    double	timeout = strtod(env_str, &end_ptr);
	    if (*end_ptr != '\0' || timeout < 0.0)
		__pmNotifyErr(LOG_WARNING, "__pmAuxConnectPMCDPort: "
			      "ignored bad PMCD_CONNECT_TIMEOUT = '%s'\n",
			      env_str);
	    else {
		canwait.tv_sec = (time_t)timeout;
		canwait.tv_usec = (int)((timeout - 
					 (double)canwait.tv_sec) * 1000000);
	    }
	}

    }
    return (&canwait);
}
 
/*
 * This interface is private to libpcp (although exposed in impl.h) and
 * deprecated (replaced by __pmAuxConnectPMCDPort()).
 * The implementation here is retained for IRIX and any 3rd party apps
 * that might have called this interface directly ... the implementation
 * is correct when $PMCD_PORT is unset, or set to a single numeric
 * port number, i.e. the old semantics
 */
int
__pmAuxConnectPMCD(const char *hostname)
{
    static int		pmcd_port;
    static int		first_time = 1;

    if (first_time) {
	char	*env_str;
	char	*end_ptr;

	first_time = 0;

	if ((env_str = getenv("PMCD_PORT")) != NULL) {

	    pmcd_port = (int)strtol(env_str, &end_ptr, 0);
	    if (*end_ptr != '\0' || pmcd_port < 0) {
		__pmNotifyErr(LOG_WARNING,
			      "__pmAuxConnectPMCD: ignored bad PMCD_PORT = '%s'\n",
			      env_str);
		pmcd_port = SERVER_PORT;
	    }
	}
	else
	    pmcd_port = SERVER_PORT;
    }

    return __pmAuxConnectPMCDPort(hostname, pmcd_port);
}

int
__pmAuxConnectPMCDPort(const char *hostname, int pmcd_port)
{
    struct sockaddr_in	myAddr;
    struct hostent*	servInfo;
    int			fd;	/* Fd for socket connection to pmcd */
    int			sts;
    int			fdFlags;

    if ((servInfo = gethostbyname(hostname)) == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "__pmAuxConnectPMCDPort(%s, %d) : hosterror=%d, ``%s''\n",
		    hostname, pmcd_port, hosterror(), hoststrerror(hosterror()));
	}
#endif
	return -EHOSTUNREACH;
    }

    __pmConnectTimeout();

    if ((fd = __pmCreateSocket()) < 0)
	return fd;

    memset(&myAddr, 0, sizeof(myAddr));	/* Arrgh! &myAddr, not myAddr */
    myAddr.sin_family = AF_INET;
    memcpy(&myAddr.sin_addr, servInfo->h_addr, servInfo->h_length);

    if ((fdFlags = __pmConnectTo(fd, (const struct sockaddr *)&myAddr,
				 pmcd_port)) < 0) {
	return (fdFlags);
    } else { /* FNDELAY and we're in progress - wait on select */
	int	rc;
	fd_set wfds;
	struct timeval stv = canwait;
	struct timeval *pstv = (stv.tv_sec || stv.tv_usec) ? &stv : NULL;

	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);
	sts = 0;
	if ((rc = select(fd+1, NULL, &wfds, NULL, pstv)) == 1) {
	    sts = __pmConnectCheckError(fd);
	}
	else if (rc == 0) {
	    sts = ETIMEDOUT;
	}
	else {
	    sts = (rc < 0) ? errno : EINVAL;
	}
	
	if (sts) {
	    close(fd);
	    return -sts;
	}
    }
    
    /* If we're here, it means we have valid connection, restore the
     * flags and make sure this file descriptor is closed if exec() is
     * called */
    return (__pmConnectRestoreFlags (fd, fdFlags));
}
