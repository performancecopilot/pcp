/*
 * Copyright (c) 2006 Aconex.  All Rights Reserved.
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

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"

static int stomp_connect(const char *hostname, int port);
static void stomp_disconnect(void);

int stomping;
char *stompfile;
extern int verbose;

static int fd = -1;
static int port = -1;
static int timeout = 2;	/* default 2 sec to timeout JMS server ACKs */
static char *hostname;
static char *username;
static char *passcode;
static char *topic;			/* JMS "topic" for pmie messages */
static char pmietopic[] = "PMIE";	/* default JMS "topic" for pmie */

static char buffer[4096];

static int stomp_connect(const char *hostname, int port)
{
    __pmSockAddr *myaddr;
    __pmHostEnt *servinfo;
    void *enumIx;
    struct timeval tv;
    struct timeval *ptv;
    __pmFdSet wfds;
    int ret;
    int flags = 0;

    if ((servinfo = __pmGetAddrInfo(hostname)) == NULL)
	return -1;

    fd = -1;
    enumIx = NULL;
    for (myaddr = __pmHostEntGetSockAddr(servinfo, &enumIx);
	 myaddr != NULL;
	 myaddr = __pmHostEntGetSockAddr(servinfo, &enumIx)) {
	/* Create a socket */
	if (__pmSockAddrIsInet(myaddr))
	    fd = __pmCreateSocket();
	else if (__pmSockAddrIsIPv6(myaddr))
	    fd = __pmCreateIPv6Socket();
	else
	    continue;
	if (fd < 0) {
	    __pmSockAddrFree(myaddr);
	    continue; /* Try the next address */
	}

	/* Attempt to connect */
	flags = __pmConnectTo(fd, myaddr, port);
	__pmSockAddrFree(myaddr);

	if (flags < 0) {
	    /*
	     * Mark failure in case we fall out the end of the loop
	     * and try next address. fd has been closed in __pmConnectTo().
	     */
	    fd = -1;
	    continue;
	}

	/* FNDELAY and we're in progress - wait on select */
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	ptv = (tv.tv_sec || tv.tv_usec) ? &tv : NULL;
	__pmFD_ZERO(&wfds);
	__pmFD_SET(fd, &wfds);
	ret = __pmSelectWrite(fd+1, &wfds, ptv);

	/* Was the connection successful? */
	if (ret <= 0) {
	    if (oserror() == EINTR)
		return -2;
	    continue;
	}
	ret = __pmConnectCheckError(fd);
	if (ret == 0)
	    break;

	/* Unsuccessful connection. */
	__pmCloseSocket(fd);
	fd = -1;
    } /* loop over addresses */

    __pmHostEntFree(servinfo);

    if(fd == -1)
	return -4;

    fd = __pmConnectRestoreFlags(fd, flags);
    if(fd < 0)
	return -5;

    return fd;
}

static int stomp_read_ack(void)
{
    struct timeval tv;
    fd_set fds, readyfds;
    int nready, sts;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    memcpy(&readyfds, &fds, sizeof(readyfds));
    nready = select(fd + 1, &readyfds, NULL, NULL, &tv);
    if (nready <= 0) {
	if (nready == 0)
	    __pmNotifyErr(LOG_ERR, "Timed out waiting for server %s:%d - %s",
				hostname, port, netstrerror());
	else
	    __pmNotifyErr(LOG_ERR, "Error waiting for server %s:%d - %s",
				hostname, port, netstrerror());
	stomp_disconnect();
	return -1;
    }

    do {
	sts = recv(fd, buffer, sizeof(buffer), 0);
	if (sts < 0) {
	    __pmNotifyErr(LOG_ERR, "Error recving from server %s:%d - %s",
				hostname, port, netstrerror());
	    stomp_disconnect();
	    return -1;
	}
	/* check for anything else we need to read to clear this ACK */
	memset(&tv, 0, sizeof(tv));
	memcpy(&readyfds, &fds, sizeof(readyfds));
    } while (select(fd + 1, &readyfds, NULL, NULL, &tv) > 0);

    return 0;
}

static int stomp_write(const char *buffer, int length)
{
    int sts;

    do {
	sts = send(fd, buffer, length, 0);
	if (sts < 0) {
	    __pmNotifyErr(LOG_ERR, "Write error to JMS server %s:%d - %s",
			hostname, port, netstrerror());
	    stomp_disconnect();
	    return -1;
	}
	else if (sts == 0)
	    break;
	length -= sts;
    } while (length > 0);

    return 0;
}

static int stomp_authenticate(void)
{
    int len;

    if (fd < 0)
	return -1;
    len = snprintf(buffer, sizeof(buffer),
		   "CONNECT\nlogin:%s\npasscode:%s\n\n", username, passcode);
    if (stomp_write(buffer, len) < 0)
	return -1;
    if (stomp_write("\0\n", 2) < 0)
	return -1;
    return 0;
}

static int stomp_destination(void)
{
    int len;

    if (fd < 0)
	return -1;
    len = snprintf(buffer, sizeof(buffer),
		   "SUB\ndestination:/topic/%s\n\n", topic);
    if (stomp_write(buffer, len) < 0)
	return -1;
    if (stomp_write("\0\n", 2) < 0)
	return -1;
    return 0;
}

static int stomp_hello(void)
{
    int len;

    if (fd < 0)
	return -1;
    len = snprintf(buffer, sizeof(buffer), "SEND\ndestination:/topic/%s\n\n"
		   "INFO: PMIE: Established initial connection", topic);
    if (stomp_write(buffer, len) < 0)
	return -1;
    if (stomp_write("\0\n", 2) < 0)
	return -1;
    return 0;
}

static void stomp_disconnect(void)
{
    if (fd >= 0)
	close(fd);
    fd = -1;
}

static char *isspace_terminate(char *string)
{
    int i = 0;

    while (!isspace((int)string[i++])) /* do nothing */ ;
    if (i)
	string[i-1] = '\0';
    return string;
}

/*
 * Parse our stomp configuration file, simple format:
 *	host=<hostname>		# JMS server machine
 *	port=<port#>		# server port number
 *	username=<user> | user=<user>
 *	passcode=<password> | password=<password>
 *	timeout=<seconds>	# optional
 *	topic=<JMStopic>	# optional
 */
static void stomp_parse(void)
{
    char config[MAXPATHLEN+1];
    FILE *f;
    int sep = __pmPathSeparator();

    if (stompfile)
	strncat(config, stompfile, sizeof(config)-1);
    else
	snprintf(config, sizeof(config),
		"%s%c" "config" "%c" "pmie" "%c" "stomp",
		 pmGetConfig("PCP_VAR_DIR"), sep, sep, sep);
    if ((f = fopen(config, "r")) == NULL) {
	__pmNotifyErr(LOG_ERR, "Cannot open STOMP configuration file %s: %s",
			config, osstrerror());
	exit(1);
    }
    while (fgets(buffer, sizeof(buffer), f)) {
	if (strncmp(buffer, "port=", 5) == 0)
	    port = atoi(isspace_terminate(&buffer[5]));
	else if (strncmp(buffer, "host=", 5) == 0)
	    hostname = strdup(isspace_terminate(&buffer[5]));
	else if (strncmp(buffer, "hostname=", 9) == 0)
	    hostname = strdup(isspace_terminate(&buffer[9]));
	else if (strncmp(buffer, "user=", 5) == 0)
	    username = strdup(isspace_terminate(&buffer[5]));
	else if (strncmp(buffer, "username=", 9) == 0)
	    username = strdup(isspace_terminate(&buffer[9]));
	else if (strncmp(buffer, "password=", 9) == 0)
	    passcode = strdup(isspace_terminate(&buffer[9]));
	else if (strncmp(buffer, "passcode=", 9) == 0)
	    passcode = strdup(isspace_terminate(&buffer[9]));
	else if (strncmp(buffer, "timeout=", 8) == 0)	/* optional */
	    timeout = atoi(isspace_terminate(&buffer[8]));
	else if (strncmp(buffer, "topic=", 6) == 0)	/* optional */
	    topic = strdup(isspace_terminate(&buffer[6]));
    }
    fclose(f);

    if (!hostname)
	__pmNotifyErr(LOG_ERR, "No host in STOMP config file %s", config);
    if (port == -1)
	__pmNotifyErr(LOG_ERR, "No port in STOMP config file %s", config);
    if (!username)
	__pmNotifyErr(LOG_ERR, "No username in STOMP config file %s", config);
    if (!passcode)
	__pmNotifyErr(LOG_ERR, "No passcode in STOMP config file %s", config);
    if (port == -1 || !hostname || !username || !passcode)
	exit(1);
}

/*
 * Setup the connection to the stomp server, and handle initial protocol
 * negotiations (sending user/passcode over to the server in particular).
 * Stomp protocol is clear text... (we don't need no stinkin' security!).
 * Note: this routine is used for both the initial connection and also for
 * any subsequent reconnect attempts.
 */
void stompInit(void)
{
    time_t thistime;
    static time_t lasttime;
    static int firsttime = 1;

    if (firsttime) {	/* initial connection attempt */
	stomp_parse();
	if (!topic)
	    topic = pmietopic;
	atexit(stomp_disconnect);
    } else {	/* reconnect attempt, if not too soon */
	time(&thistime);
	if (thistime < lasttime + 60)
	    goto disconnect;
    }

    if (verbose)
	__pmNotifyErr(LOG_INFO, "Connecting to %s, port %d", hostname, port);
    if (stomp_connect(hostname, port) < 0) {
	__pmNotifyErr(LOG_ERR, "Could not connect to the message server");
	goto disconnect;
    }

    if (verbose)
	__pmNotifyErr(LOG_INFO, "Connected; sending stomp connect message");
    if (stomp_authenticate() < 0) {
	__pmNotifyErr(LOG_ERR, "Could not sent STOMP CONNECT frame to server");
	goto disconnect;
    }

    if (verbose)
	__pmNotifyErr(LOG_INFO, "Sent; waiting for server ACK");
    if (stomp_read_ack() < 0) {
	__pmNotifyErr(LOG_ERR, "Could not read STOMP ACK frame.");
	goto disconnect;
    }

    if (verbose)
	__pmNotifyErr(LOG_INFO, "ACK; sending initial PMIE topic and hello");
    if (stomp_destination() < 0) {
	__pmNotifyErr(LOG_ERR, "Could not read TOPIC frame.");
	goto disconnect;
    }
    if (stomp_hello() < 0) {
	__pmNotifyErr(LOG_ERR, "Could not send HELLO frame.");
	goto disconnect;
    }

    if (verbose)
	__pmNotifyErr(LOG_INFO, "Sent; waiting for server ACK");
    if (stomp_read_ack() < 0) {
	 __pmNotifyErr(LOG_ERR, "Could not read STOMP ACK frame");
	goto disconnect;
    }

    if (!firsttime)
	__pmNotifyErr(LOG_INFO, "Reconnected to STOMP protocol server");
    else if (verbose)
	__pmNotifyErr(LOG_INFO, "Initial STOMP protocol setup complete");
    firsttime = 0;
    goto finished;

disconnect:
    stomp_disconnect();
    if (firsttime)
	exit(1);
    /* otherwise, we attempt reconnect on next message firing (>1min) */
finished:
    lasttime = thistime;
}

/*
 * Send a message to the stomp server.
 */
int stompSend(const char *msg)
{
    int len;

    if (fd < 0) stompInit();	/* reconnect */
    if (fd < -1) return -1;

    len = snprintf(buffer, sizeof(buffer),
		   "SEND\ndestination:/topic/%s\n\n", topic);
    if (stomp_write(buffer, len) < 0)
	return -1;
    if (stomp_write(msg, strlen(msg)) < 0)
	return -1;
    if (stomp_write("\0\n", 2) < 0)
	return -1;
    return 0;
}
