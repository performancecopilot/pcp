/*
 * Copyright (c) 2012-2019 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include "pmapi.h"
#include "libpcp.h"
#include "pmproxy.h"

/* The table of clients, used by pmproxy */
typedef struct {
    int			fd;		/* client socket descriptor */
    int			version;	/* proxy-client protocol version */
    struct {				/* Status of connection to client */
	unsigned int	connected : 1;	/* Client connected, socket level */
	unsigned int	allowed : 1;	/* Creds seen, OK to talk to pmcd */
    } status;
    char		*pmcd_hostname;	/* PMCD hostname */
    int			pmcd_port;	/* PMCD port */
    int			pmcd_fd;	/* PMCD socket file descriptor */
    __pmSockAddr	*addr;		/* address of client */
    unsigned int	server_features;/* features the server is advertising */
} ClientInfo;

typedef struct {
    ClientInfo		*client;
    int			nClients;	/* Number in array, (not all in use) */
    int			clientSize;	/* Client allocation highwater mark */
    int			maxReqPortFd;	/* highest request port fd */
    int			maxSockFd;	/* largest fd for a client */
    __pmFdSet		sockFds;	/* for client select() */
} ServerInfo;

#define FDNAMELEN		40	/* maximum length of a fd description */
#define MIN_CLIENTS_ALLOC	8

static int	timeToDie;		/* for SIGINT handling */
#ifdef HAVE_SA_SIGINFO
static pid_t    killer_pid;
static uid_t    killer_uid;
#endif
static int      killer_sig;

static void
SignalShutdown(void)
{
#ifdef HAVE_SA_SIGINFO
#if DESPERATE
    char	buf[256];
#endif
    if (killer_pid != 0) {
	pmNotifyErr(LOG_INFO, "pmproxy caught %s from pid=%" FMT_PID " uid=%d\n",
	    killer_sig == SIGINT ? "SIGINT" : "SIGTERM", killer_pid, killer_uid);
#if DESPERATE
	pmNotifyErr(LOG_INFO, "Try to find process in ps output ...\n");
	pmsprintf(buf, sizeof(buf), "sh -c \". \\$PCP_DIR/etc/pcp.env; ( \\$PCP_PS_PROG \\$PCP_PS_ALL_FLAGS | \\$PCP_AWK_PROG 'NR==1 {print} \\$2==%" FMT_PID " {print}' )\"", killer_pid);
	system(buf);
#endif
    }
    else {
	pmNotifyErr(LOG_INFO, "pmproxy caught %s from unknown process\n",
			killer_sig == SIGINT ? "SIGINT" : "SIGTERM");
    }
#else
    pmNotifyErr(LOG_INFO, "pmproxy caught %s\n",
		killer_sig == SIGINT ? "SIGINT" : "SIGTERM");
#endif
    Shutdown();
    exit(0);
}

#ifdef HAVE_SA_SIGINFO
static void
SigIntProc(int sig, siginfo_t *sip, void *x)
{
    killer_sig = sig;
    if (sip != NULL) {
	killer_pid = sip->si_pid;
	killer_uid = sip->si_uid;
    }
    timeToDie = 1;
}
#elif IS_MINGW
static void
SigIntProc(int sig)
{
    SignalShutdown();
}
#else
static void
SigIntProc(int sig)
{
    killer_sig = sig;
    signal(SIGINT, SigIntProc);
    signal(SIGTERM, SigIntProc);
    timeToDie = 1;
}
#endif

void
SigBad(int sig)
{
    if (pmDebugOptions.desperate) {
	pmNotifyErr(LOG_ERR, "Unexpected signal %d ...\n", sig);
	__pmDumpStack();
	fflush(stderr);
	fprintf(stderr, "\nDumping to core ...\n");
    }
    _exit(sig);
}

static int
NewClient(ServerInfo *sp)
{
    ClientInfo	*client;
    int		i;

    for (i = 0; i < sp->nClients; i++)
	if (!sp->client[i].status.connected)
	    break;

    if (i == sp->clientSize) {
	int j, sz;

	if (sp->clientSize)
	    sp->clientSize = sp->clientSize * 2;
	else
	    sp->clientSize = MIN_CLIENTS_ALLOC;
	sz = sizeof(ClientInfo) * sp->clientSize;
	client = (ClientInfo *)realloc(sp->client, sz);
	if (client == NULL) {
	    pmNoMem("NewClient", sz, PM_RECOV_ERR);
	    Shutdown();
	    exit(1);
	}
	sp->client = client;
	for (j = i; j < sp->clientSize; j++)
	    sp->client[j].addr = NULL;
    }
    sp->client[i].addr = __pmSockAddrAlloc();
    if (sp->client[i].addr == NULL) {
        pmNoMem("NewClient", __pmSockAddrSize(), PM_RECOV_ERR);
	Shutdown();
	exit(1);
    }
    if (i >= sp->nClients)
	sp->nClients = i + 1;
    return i;
}

static void
DeleteClient(ServerInfo *sp, ClientInfo *cp)
{
    int		i;

    for (i = 0; i < sp->nClients; i++)
	if (cp == &sp->client[i])
	    break;
    if (i == sp->nClients) {
	fprintf(stderr,
		"DeleteClient: Botch: tried to delete non-existent client "
		"@" PRINTF_P_PFX "%p\n", cp);
	return;
    }

    if (pmDebugOptions.context)
	fprintf(stderr, "DeleteClient [%d]\n", i);

    if (cp->fd >= 0) {
	__pmFD_CLR(cp->fd, &sp->sockFds);
	__pmCloseSocket(cp->fd);
    }
    if (cp->pmcd_fd >= 0) {
	__pmFD_CLR(cp->pmcd_fd, &sp->sockFds);
	__pmCloseSocket(cp->pmcd_fd);
    }
    if (i == sp->nClients-1) {
	i--;
	while (i >= 0 && !sp->client[i].status.connected)
	    i--;
	sp->nClients = (i >= 0) ? i + 1 : 0;
    }
    if (cp->fd == sp->maxSockFd || cp->pmcd_fd == sp->maxSockFd) {
	sp->maxSockFd = sp->maxReqPortFd;
	for (i = 0; i < sp->nClients; i++) {
	    if (cp->status.connected == 0)
		continue;
	    if (sp->client[i].fd > sp->maxSockFd)
		sp->maxSockFd = sp->client[i].fd;
	    if (sp->client[i].pmcd_fd > sp->maxSockFd)
		sp->maxSockFd = sp->client[i].pmcd_fd;
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

static void
CleanupClient(ServerInfo *sp, ClientInfo *cp, int sts)
{
    if (pmDebugOptions.appl0) {
	int		i;

	for (i = 0; i < sp->nClients; i++) {
	    if (cp == &sp->client[i])
		break;
	}
	fprintf(stderr, "CleanupClient: client[%d] fd=%d %s (%d)\n",
	    i, cp->fd, pmErrStr(sts), sts);
    }
    DeleteClient(sp, cp);
}

/* MY_BUFLEN needs to big enough to hold "hostname port" */
#define MY_BUFLEN (MAXHOSTNAMELEN+10)
#define MY_VERSION "pmproxy-server 1\n"

/* Establish a new socket connection to a client */
static ClientInfo *
AcceptNewClient(ServerInfo *sp, int reqfd)
{
    int		i;
    int		fd;
    __pmSockLen	addrlen;
    int		ok = 0;
    char	buf[MY_BUFLEN];
    char	*bp;
    char	*endp;
    char	*abufp;

    i = NewClient(sp);
    addrlen = __pmSockAddrSize();
    fd = __pmAccept(reqfd, sp->client[i].addr, &addrlen);
    if (fd == -1) {
	pmNotifyErr(LOG_ERR, "AcceptNewClient(%d) __pmAccept failed: %s",
			reqfd, netstrerror());
	Shutdown();
	exit(1);
    }
    __pmSetSocketIPC(fd);
    if (fd > sp->maxSockFd)
	sp->maxSockFd = fd;
    __pmFD_SET(fd, &sp->sockFds);

    sp->client[i].fd = fd;
    sp->client[i].pmcd_fd = -1;
    sp->client[i].status.connected = 1;
    sp->client[i].status.allowed = 0;
    sp->client[i].pmcd_hostname = NULL;

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
	    sp->client[i].version = 1;
	    ok = 1;
	}
    }

    if (!ok) {
	if (pmDebugOptions.context) {
	    abufp = __pmSockAddrToString(sp->client[i].addr);
	    pmNotifyErr(LOG_INFO, "Bad version string from client at %s",
			abufp);
	    free(abufp);
	    fprintf(stderr, "AcceptNewClient: bad version string was \"");
	    for (bp = buf; *bp && bp < &buf[MY_BUFLEN]; bp++)
		fputc(*bp & 0xff, stderr);
	    fprintf(stderr, "\"\n");
	}
	DeleteClient(sp, &sp->client[i]);
	return NULL;
    }

    if (__pmSend(fd, MY_VERSION, strlen(MY_VERSION), 0) != strlen(MY_VERSION)) {
	abufp = __pmSockAddrToString(sp->client[i].addr);
	pmNotifyErr(LOG_WARNING, "AcceptNewClient: failed to send version "
			"string (%s) to client at %s\n", MY_VERSION, abufp);
	free(abufp);
	DeleteClient(sp, &sp->client[i]);
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
	    sp->client[i].pmcd_hostname = strdup(buf);
	    if (sp->client[i].pmcd_hostname == NULL)
		pmNoMem("PMCD.hostname", strlen(buf), PM_FATAL_ERR);
	    bp++;
	    sp->client[i].pmcd_port = (int)strtoul(bp, &endp, 10);
	    if (*endp != '\0') {
		abufp = __pmSockAddrToString(sp->client[i].addr);
		pmNotifyErr(LOG_WARNING, "AcceptNewClient: bad pmcd port "
				"\"%s\" from client at %s", bp, abufp);
		free(abufp);
		DeleteClient(sp, &sp->client[i]);
		return NULL;
	    }
	}
	/* error, fall through */
    }

    if (sp->client[i].pmcd_hostname == NULL) {
	abufp = __pmSockAddrToString(sp->client[i].addr);
	pmNotifyErr(LOG_WARNING, "AcceptNewClient: failed to get PMCD "
				"hostname (%s) from client at %s", buf, abufp);
	free(abufp);
	DeleteClient(sp, &sp->client[i]);
	return NULL;
    }

    if (pmDebugOptions.context) {
	/* note error message gets appended to once pmcd connection is made */
	abufp = __pmSockAddrToString(sp->client[i].addr);
	fprintf(stderr, "AcceptNewClient [%d] fd=%d from %s to %s (port %s)",
		i, fd, abufp, sp->client[i].pmcd_hostname, bp);
	free(abufp);
    }

    return &sp->client[i];
}

static int
CheckCertRequired(ClientInfo *cp)
{
    if ((cp->server_features & PDU_FLAG_CERT_REQD) &&
	!__pmSockAddrIsLoopBack(cp->addr) &&
	!__pmSockAddrIsUnix(cp->addr))
	return 1;
    return 0;
}

static int
VerifyClient(ClientInfo *cp, __pmPDU *pb)
{
    int	i, sts, flags = 0, sender = 0, credcount = 0;
    __pmPDUHdr *header = (__pmPDUHdr *)pb;
    __pmHashCtl attrs = { 0 };
    __pmCred *credlist;

    /* first check that this is a credentials PDU */
    if (header->type != PDU_CREDS)
	return PM_ERR_IPC;

    /* now decode it and if secure connection requested, set it up */
    if ((sts = __pmDecodeCreds(pb, &sender, &credcount, &credlist)) < 0)
	return sts;

    for (i = 0; i < credcount; i++) {
	if (credlist[i].c_type == CVERSION) {
	    __pmVersionCred *vcp = (__pmVersionCred *)&credlist[i];
	    flags = vcp->c_flags;
	    break;
	}
    }

    if (credlist != NULL)
	free(credlist);

    /*
     * If the server advertises PDU_FLAG_CERT_REQD, add it to flags
     * so we can setup the connection properly with the client.
     *
     * In normal operation, some of this code is redundant. A 
     * remote client should error out during initial handshake
     * if it does not support client certs.
     *
     * We still need to check local connections and allow those through
     * in all cases.
     */

    if (CheckCertRequired(cp)) {
	if (flags & PDU_FLAG_SECURE)
	    flags |= PDU_FLAG_CERT_REQD;
	else
	    return PM_ERR_NEEDCLIENTCERT;
    }

    /* need to ensure both the pmcd and client channel use flags */

    if (sts >= 0 && flags)
	sts = __pmSecureServerHandshake(cp->fd, flags, &attrs);

    /* send credentials PDU through to pmcd now (order maintained) */
    if (sts >= 0)
	sts = __pmXmitPDU(cp->pmcd_fd, pb);

    /*
     * finally perform any additional handshaking needed with pmcd.
     * Do not initialize NSS again.
     */
    if (sts >= 0 && flags)
	sts = __pmSecureClientHandshake(cp->pmcd_fd,
					flags | PDU_FLAG_NO_NSS_INIT,
					cp->pmcd_hostname, &attrs);
   
    return sts;
}

/* Determine which clients (if any) have sent data to the server and handle it
 * as required.
 */
static void
HandleInput(ServerInfo *sp, __pmFdSet *fdsPtr)
{
    int		i, sts;
    __pmPDU	*pb;
    ClientInfo	*cp;

    /* input from clients */
    for (i = 0; i < sp->nClients; i++) {
	if (!sp->client[i].status.connected || !__pmFD_ISSET(sp->client[i].fd, fdsPtr))
	    continue;

	cp = &sp->client[i];

	sts = __pmGetPDU(cp->fd, LIMIT_SIZE, 0, &pb);
	if (sts <= 0) {
	    CleanupClient(sp, cp, sts);
	    continue;
	}

	/* We *must* see a credentials PDU as the first PDU */
	if (!cp->status.allowed) {
	    sts = VerifyClient(cp, pb);
	    __pmUnpinPDUBuf(pb);
	    if (sts < 0) {
		CleanupClient(sp, cp, sts);
		continue;
	    }
	    cp->status.allowed = 1;
	    continue;
	}

	sts = __pmXmitPDU(cp->pmcd_fd, pb);
	__pmUnpinPDUBuf(pb);
	if (sts <= 0) {
	    CleanupClient(sp, cp, sts);
	    continue;
	}
    }

    /* input from pmcds */
    for (i = 0; i < sp->nClients; i++) {
	if (!sp->client[i].status.connected ||
	    !__pmFD_ISSET(sp->client[i].pmcd_fd, fdsPtr))
	    continue;

	cp = &sp->client[i];

	sts = __pmGetPDU(cp->pmcd_fd, ANY_SIZE, 0, &pb);

	/*
	 * We need to know if the pmcd has PDU_FLAG_CERT_REQD so we can
	 * setup our own secure connection with the client. Need to intercept
	 * the first message from the pmcd.  See __pmConnectHandshake
	 * discussion in connect.c. This code happens before VerifyClient
	 * above.
	 */

	if ((!cp->status.allowed) && (sts == PDU_ERROR)) {
	    unsigned int server_features;

	    server_features = __pmServerGetFeaturesFromPDU(pb);
	    if (server_features & PDU_FLAG_CERT_REQD) {
		/* Add as a server feature */
		cp->server_features |= PDU_FLAG_CERT_REQD;
	    }
	}

	if (sts <= 0) {
	    CleanupClient(sp, cp, sts);
	    continue;
	}

	sts = __pmXmitPDU(cp->fd, pb);
	__pmUnpinPDUBuf(pb);
	if (sts <= 0) {
	    CleanupClient(sp, cp, sts);
	    continue;
	}
    }
}

static void
CheckNewClient(__pmFdSet * fdset, int rfd, int family)
{
    ClientInfo	*cp;
    ServerInfo	*sp = (ServerInfo *)GetServerInfo();

    if (__pmFD_ISSET(rfd, fdset)) {
	if ((cp = AcceptNewClient(sp, rfd)) == NULL)
	    /* failed to negotiate, already cleaned up */
	    return;

	/* establish a new connection to pmcd */
	if ((cp->pmcd_fd = __pmAuxConnectPMCDPort(cp->pmcd_hostname, cp->pmcd_port)) < 0) {
	    if (pmDebugOptions.context)
		/* append to message started in AcceptNewClient() */
		fprintf(stderr, " oops!\n"
			"__pmAuxConnectPMCDPort(%s,%d) failed: %s\n",
			cp->pmcd_hostname, cp->pmcd_port,
			pmErrStr(-oserror()));
	    CleanupClient(sp, cp, -oserror());
	}
	else {
	    if (cp->pmcd_fd > sp->maxSockFd)
		sp->maxSockFd = cp->pmcd_fd;
	    __pmFD_SET(cp->pmcd_fd, &sp->sockFds);
	    if (pmDebugOptions.context)
		/* append to message started in AcceptNewClient() */
		fprintf(stderr, " fd=%d\n", cp->pmcd_fd);
	}
    }
}

static char *
FdToString(ServerInfo *sp, int fd)
{
    static char fdStr[FDNAMELEN];
    static char *stdFds[4] = {"*UNKNOWN FD*", "stdin", "stdout", "stderr"};
    ClientInfo	*cp;
    int		i;

    if (fd >= -1 && fd < 3)
	return stdFds[fd + 1];
    if (__pmServerRequestPortString(fd, fdStr, FDNAMELEN) != NULL)
	return fdStr;
    for (i = 0; i < sp->nClients; i++) {
	cp = &sp->client[i];
	if (cp->status.connected && fd == cp->fd) {
	    pmsprintf(fdStr, FDNAMELEN, "client[%d] client socket", i);
	    return fdStr;
	}
	if (cp->status.connected && fd == cp->pmcd_fd) {
	    pmsprintf(fdStr, FDNAMELEN, "client[%d] pmcd socket", i);
	    return fdStr;
	}
    }
    return stdFds[0];
}

/* Loop, synchronously processing requests from clients. */
static void
MainLoop(void *arg, struct timeval *runtime)
{
    int		i, sts;
    int		maxFd;
    __pmFdSet	readableFds;
    ServerInfo	*sp = (ServerInfo *)arg;

    (void)runtime;

    for (;;) {
	/* Figure out which file descriptors to wait for input on.  Keep
	 * track of the highest numbered descriptor for the select call.
	 */
	readableFds = sp->sockFds;
	maxFd = sp->maxSockFd + 1;

	sts = __pmSelectRead(maxFd, &readableFds, NULL);

	if (sts > 0) {
	    if (pmDebugOptions.appl0)
		for (i = 0; i <= sp->maxSockFd; i++)
		    if (__pmFD_ISSET(i, &readableFds))
			fprintf(stderr, "__pmSelectRead(): from %s fd=%d\n",
				FdToString(sp, i), i);
	    __pmServerAddNewClients(&readableFds, CheckNewClient);
	    HandleInput(sp, &readableFds);
	}
	else if (sts == -1 && neterror() != EINTR) {
	    pmNotifyErr(LOG_ERR, "ClientLoop select: %s\n", netstrerror());
	    break;
	}
	if (timeToDie) {
	    SignalShutdown();
	    break;
	}
    }
}

static void
DumpRequestPorts(FILE *stream, void *arg)
{
    (void)arg;
    __pmServerDumpRequestPorts(stderr);
}

static void
ShutdownPorts(void *arg)
{
    ServerInfo	*sp = (ServerInfo *)arg;
    int		i;

    for (i = 0; i < sp->nClients; i++)
	if (sp->client[i].status.connected)
	    __pmCloseSocket(sp->client[i].fd);
    __pmServerCloseRequestPorts();
}

static void *
OpenRequestPorts(char *path, size_t pathlen, int maxpending)
{
    ServerInfo	*sp;
    int		sts;
#ifdef HAVE_SA_SIGINFO
    static struct sigaction act;
#endif

#ifdef HAVE_SA_SIGINFO
    act.sa_sigaction = SigIntProc;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
#else
    __pmSetSignalHandler(SIGINT, SigIntProc);
    __pmSetSignalHandler(SIGTERM, SigIntProc);
#endif
    __pmSetSignalHandler(SIGHUP, SIG_IGN);
    __pmSetSignalHandler(SIGBUS, SigBad);
    __pmSetSignalHandler(SIGSEGV, SigBad);

    (void)path;
    (void)pathlen;

    if ((sp = calloc(1, sizeof(ServerInfo))) == NULL)
	return NULL;
    if ((sts = __pmServerOpenRequestPorts(&sp->sockFds, maxpending)) < 0) {
	free(sp);
	return NULL;
    }
    sp->maxReqPortFd = sp->maxSockFd = sts;
    return sp;
}

struct pmproxy libpcp_pmproxy = {
    .openports	= OpenRequestPorts,
    .dumpports	= DumpRequestPorts,
    .shutdown	= ShutdownPorts,
    .loop 	= MainLoop,
};
