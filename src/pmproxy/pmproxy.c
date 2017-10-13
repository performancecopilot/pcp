/*
 * Copyright (c) 2012-2015 Red Hat.
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <sys/stat.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#define MAXPENDING	5	/* maximum number of pending connections */
#define FDNAMELEN	40	/* maximum length of a fd description */
#define STRINGIFY(s)    #s
#define TO_STRING(s)    STRINGIFY(s)

static char	*FdToString(int);

static int	timeToDie;		/* For SIGINT handling */
static char	*logfile = "pmproxy.log";	/* log file name */
static int	run_daemon = 1;		/* run as a daemon, see -f */
static char	*fatalfile = "/dev/tty";/* fatal messages at startup go here */
static char	*username;
static char	*certdb;		/* certificate DB path (NSS) */
static char	*dbpassfile;		/* certificate DB password file */
static char     *cert_nickname;         /* Alternate nickname to use for server certificate */
static char	*hostname;

static void
DontStart(void)
{
    FILE	*tty;
    FILE	*log;

    __pmNotifyErr(LOG_ERR, "pmproxy not started due to errors!\n");

    if ((tty = fopen(fatalfile, "w")) != NULL) {
	fflush(stderr);
	fprintf(tty, "NOTE: pmproxy not started due to errors!  ");
	if ((log = fopen(logfile, "r")) != NULL) {
	    int		c;
	    fprintf(tty, "Log file \"%s\" contains ...\n", logfile);
	    while ((c = fgetc(log)) != EOF)
		fputc(c, tty);
	    fclose(log);
	}
	else
	    fprintf(tty, "Log file \"%s\" has vanished!\n", logfile);
	fclose(tty);
    }
    exit(1);
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_DEBUG,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Service options"),
    { "", 0, 'A', 0, "disable service advertisement" },
    { "foreground", 0, 'f', 0, "run in the foreground" },
    { "username", 1, 'U', "USER", "in daemon mode, run as named user [default pcp]" },
    PMAPI_OPTIONS_HEADER("Configuration options"),
    { "certdb", 1, 'C', "PATH", "path to NSS certificate database" },
    { "passfile", 1, 'P', "PATH", "password file for certificate database access" },
    { "", 1, 'L', "BYTES", "maximum size for PDUs from clients [default 65536]" },
    PMAPI_OPTIONS_HEADER("Connection options"),
    { "interface", 1, 'i', "ADDR", "accept connections on this IP address" },
    { "port", 1, 'p', "N", "accept connections on this port" },
    PMAPI_OPTIONS_HEADER("Diagnostic options"),
    { "log", 1, 'l', "PATH", "redirect diagnostics and trace output" },
    { "", 1, 'x', "PATH", "fatal messages at startup sent to file [default /dev/tty]" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "A:C:D:fi:l:L:M:p:P:U:x:?",
    .long_options = longopts,
};

static void
ParseOptions(int argc, char *argv[], int *nports)
{
    int		c;
    int		sts;
    int		usage = 0;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'A':   /* disable pmproxy service advertising */
	    __pmServerClearFeature(PM_SERVER_FEATURE_DISCOVERY);
	    break;

	case 'C':	/* path to NSS certificate database */
	    certdb = opts.optarg;
	    break;

	case 'D':	/* debug options */
	    if ((sts = pmSetDebug(opts.optarg)) < 0) {
		pmprintf("%s: unrecognized debug options specification (%s)\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    }
	    break;

	case 'f':	/* foreground, i.e. do _not_ run as a daemon */
	    run_daemon = 0;
	    break;

	case 'i':
	    /* one (of possibly several) interfaces for client requests */
	    __pmServerAddInterface(opts.optarg);
	    break;

	case 'l':
	    /* log file name */
	    logfile = opts.optarg;
	    break;

        case 'M':   /* nickname for the server cert. Use to query the nssdb */
            cert_nickname = opts.optarg;
            break;

	case 'L': /* Maximum size for PDUs from clients */
	    sts = (int)strtol(opts.optarg, NULL, 0);
	    if (sts <= 0) {
		pmprintf("%s: -L requires a positive value\n", pmProgname);
		opts.errors++;
	    } else {
		__pmSetPDUCeiling(sts);
	    }
	    break;

	case 'p':
	    if (__pmServerAddPorts(opts.optarg) < 0) {
		pmprintf("%s: -p requires a positive numeric argument (%s)\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    } else {
		*nports += 1;
	    }
	    break;

	case 'P':	/* password file for certificate database access */
	    dbpassfile = opts.optarg;
	    break;

	case 'U':	/* run as user username */
	    username = opts.optarg;
	    break;

	case 'x':
	    fatalfile = opts.optarg;
	    break;

	case '?':
	    usage = 1;
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }

    if (usage || opts.errors || opts.optind < argc) {
	pmUsageMessage(&opts);
	if (usage)
	    exit(0);
	DontStart();
    }
}

static int
CheckCertRequired(ClientInfo *cp)
{
    if( cp->server_features & PDU_FLAG_CERT_REQD )
	if ( !__pmSockAddrIsLoopBack(cp->addr) && !__pmSockAddrIsUnix(cp->addr) )
	    return 1;

    return 0;
}

static void
CleanupClient(ClientInfo *cp, int sts)
{
    if (pmDebugOptions.appl0) {
	int		i;
	for (i = 0; i < nClients; i++) {
	    if (cp == &client[i])
		break;
	}
	fprintf(stderr, "CleanupClient: client[%d] fd=%d %s (%d)\n",
	    i, cp->fd, pmErrStr(sts), sts);
    }

    DeleteClient(cp);
}

static int
VerifyClient(ClientInfo *cp, __pmPDU *pb)
{
    int	i, sts, flags = 0, sender = 0, credcount = 0;
    __pmPDUHdr *header = (__pmPDUHdr *)pb;
    __pmHashCtl attrs = { 0 }; /* TODO */
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

    if ( CheckCertRequired(cp) ){
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


/* This is a private libpcp function.  OK to expose ? Copied for now */
__pmPDUInfo
__ntohpmPDUInfo(__pmPDUInfo info)
{
    unsigned int        x;

    x = ntohl(*(unsigned int *)&info);
    info = *(__pmPDUInfo *)&x;

    return info;
}

/* Determine which clients (if any) have sent data to the server and handle it
 * as required.
 */
void
HandleInput(__pmFdSet *fdsPtr)
{
    int		i, sts;
    __pmPDU	*pb;
    ClientInfo	*cp;

    /* input from clients */
    for (i = 0; i < nClients; i++) {
	if (!client[i].status.connected || !__pmFD_ISSET(client[i].fd, fdsPtr))
	    continue;

	cp = &client[i];

	sts = __pmGetPDU(cp->fd, LIMIT_SIZE, 0, &pb);
	if (sts <= 0) {
	    CleanupClient(cp, sts);
	    continue;
	}

	/* We *must* see a credentials PDU as the first PDU */
	if (!cp->status.allowed) {
	    sts = VerifyClient(cp, pb);
	    __pmUnpinPDUBuf(pb);
	    if (sts < 0) {
		CleanupClient(cp, sts);
		continue;
	    }
	    cp->status.allowed = 1;
	    continue;
	}

	sts = __pmXmitPDU(cp->pmcd_fd, pb);
	__pmUnpinPDUBuf(pb);
	if (sts <= 0) {
	    CleanupClient(cp, sts);
	    continue;
	}
    }

    /* input from pmcds */
    for (i = 0; i < nClients; i++) {
	if (!client[i].status.connected ||
	    !__pmFD_ISSET(client[i].pmcd_fd, fdsPtr))
	    continue;

	cp = &client[i];

	sts = __pmGetPDU(cp->pmcd_fd, ANY_SIZE, 0, &pb);

	/*
	 * We need to know if the pmcd has PDU_FLAG_CERT_REQD so we can
	 * setup our own secure connection with the client. Need to intercept
	 * the first message from the pmcd.  See __pmConnectHandshake
	 * discussion in connect.c. This code happens before VerifyClient
	 * above.
	 */

	if( (!cp->status.allowed) && (sts == PDU_ERROR) ){
	    unsigned int server_features;
	    server_features = __pmServerGetFeaturesFromPDU( pb );
	    if( server_features & PDU_FLAG_CERT_REQD ){
		/* Add as a server feature */
		cp->server_features |= PDU_FLAG_CERT_REQD;
	    }
	}

	if (sts <= 0) {
	    CleanupClient(cp, sts);
	    continue;
	}

	sts = __pmXmitPDU(cp->fd, pb);
	__pmUnpinPDUBuf(pb);
	if (sts <= 0) {
	    CleanupClient(cp, sts);
	    continue;
	}
    }
}

/* Called to shutdown pmproxy in an orderly manner */
void
Shutdown(void)
{
    int	i;

    for (i = 0; i < nClients; i++)
	if (client[i].status.connected)
	    __pmCloseSocket(client[i].fd);
    __pmServerCloseRequestPorts();
    __pmSecureServerShutdown();
    __pmNotifyErr(LOG_INFO, "pmproxy Shutdown\n");
    fflush(stderr);
}

void
SignalShutdown(void)
{
    __pmNotifyErr(LOG_INFO, "pmproxy caught SIGINT or SIGTERM\n");
    Shutdown();
    exit(0);
}

static void
CheckNewClient(__pmFdSet * fdset, int rfd, int family)
{
    ClientInfo	*cp;

    if (__pmFD_ISSET(rfd, fdset)) {
	if ((cp = AcceptNewClient(rfd)) == NULL)
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
	    CleanupClient(cp, -oserror());
	}
	else {
	    if (cp->pmcd_fd > maxSockFd)
		maxSockFd = cp->pmcd_fd;
	    __pmFD_SET(cp->pmcd_fd, &sockFds);
	    if (pmDebugOptions.context)
		/* append to message started in AcceptNewClient() */
		fprintf(stderr, " fd=%d\n", cp->pmcd_fd);
	}
    }
}

/* Loop, synchronously processing requests from clients. */
static void
ClientLoop(void)
{
    int		i, sts;
    int		maxFd;
    __pmFdSet	readableFds;

    for (;;) {
	/* Figure out which file descriptors to wait for input on.  Keep
	 * track of the highest numbered descriptor for the select call.
	 */
	readableFds = sockFds;
	maxFd = maxSockFd + 1;

	sts = __pmSelectRead(maxFd, &readableFds, NULL);

	if (sts > 0) {
	    if (pmDebugOptions.appl0)
		for (i = 0; i <= maxSockFd; i++)
		    if (__pmFD_ISSET(i, &readableFds))
			fprintf(stderr, "__pmSelectRead(): from %s fd=%d\n",
				FdToString(i), i);
	    __pmServerAddNewClients(&readableFds, CheckNewClient);
	    HandleInput(&readableFds);
	}
	else if (sts == -1 && neterror() != EINTR) {
	    __pmNotifyErr(LOG_ERR, "ClientLoop select: %s\n", netstrerror());
	    break;
	}
	if (timeToDie) {
	    SignalShutdown();
	    break;
	}
    }
}

#ifdef IS_MINGW
static void
SigIntProc(int s)
{
    SignalShutdown();
}
#else
static void
SigIntProc(int s)
{
    signal(SIGINT, SigIntProc);
    signal(SIGTERM, SigIntProc);
    timeToDie = 1;
}
#endif

static void
SigBad(int sig)
{
    if (pmDebugOptions.desperate) {
	__pmNotifyErr(LOG_ERR, "Unexpected signal %d ...\n", sig);
	fprintf(stderr, "\nDumping to core ...\n");
	fflush(stderr);
    }
    _exit(sig);
}

/*
 * Hostname extracted and cached for later use during protocol negotiations
 */
static void
GetProxyHostname(void)
{
    __pmHostEnt	*hep;
    char        host[MAXHOSTNAMELEN];

    if (gethostname(host, MAXHOSTNAMELEN) < 0) {
        __pmNotifyErr(LOG_ERR, "%s: gethostname failure\n", pmProgname);
        DontStart();
    }
    host[MAXHOSTNAMELEN-1] = '\0';

    hep = __pmGetAddrInfo(host);
    if (hep == NULL) {
        __pmNotifyErr(LOG_ERR, "%s: __pmGetAddrInfo failure\n", pmProgname);
        DontStart();
    } else {
        hostname = __pmHostEntGetName(hep);
        if (!hostname) {	/* no reverse DNS lookup for local hostname */
            hostname = strdup(host);
            if (!hostname)	/* out of memory, we're having a bad day!?! */
                __pmNoMem("PMPROXY.hostname", strlen(host), PM_FATAL_ERR);
        }
        __pmHostEntFree(hep);
    }
}

int
main(int argc, char *argv[])
{
    int		sts;
    int		nport = 0;
    int		localhost = 0;
    int		maxpending = MAXPENDING;
    char	*envstr;

    umask(022);
    __pmGetUsername(&username);
    __pmSetInternalState(PM_STATE_PMCS);
    __pmServerSetFeature(PM_SERVER_FEATURE_DISCOVERY);

    if ((envstr = getenv("PMPROXY_PORT")) != NULL)
	nport = __pmServerAddPorts(envstr);
    if ((envstr = getenv("PMPROXY_LOCAL")) != NULL)
	if ((localhost = atoi(envstr)) != 0)
	    __pmServerSetFeature(PM_SERVER_FEATURE_LOCAL);
    if ((envstr = getenv("PMPROXY_MAXPENDING")) != NULL)
	maxpending = atoi(envstr);
    ParseOptions(argc, argv, &nport);

    __pmOpenLog(pmProgname, logfile, stderr, &sts);
    /* close old stdout, and force stdout into same stream as stderr */
    fflush(stdout);
    close(fileno(stdout));
    if (dup(fileno(stderr)) == -1) {
	fprintf(stderr, "Warning: dup() failed: %s\n", pmErrStr(-oserror()));
    }

    if (localhost)
	__pmServerAddInterface("INADDR_LOOPBACK");
    if (nport == 0)
        __pmServerAddPorts(TO_STRING(PROXY_PORT));
    GetProxyHostname();

    __pmServerSetServiceSpec(PM_SERVER_PROXY_SPEC);
    if (run_daemon) {
	__pmServerStart(argc, argv, 1);
	__pmServerCreatePIDFile(PM_SERVER_PROXY_SPEC, 0);
    }

    __pmSetSignalHandler(SIGHUP, SIG_IGN);
    __pmSetSignalHandler(SIGINT, SigIntProc);
    __pmSetSignalHandler(SIGTERM, SigIntProc);
    __pmSetSignalHandler(SIGBUS, SigBad);
    __pmSetSignalHandler(SIGSEGV, SigBad);

    /* Open request ports for client connections */
    if ((sts = __pmServerOpenRequestPorts(&sockFds, maxpending)) < 0)
	DontStart();
    maxReqPortFd = maxSockFd = sts;

    /* lose root privileges if we have them */
    __pmSetProcessIdentity(username);

    fprintf(stderr, "pmproxy: PID = %" FMT_PID, (pid_t)getpid());
    fprintf(stderr, ", PDU version = %u", PDU_VERSION);
#ifdef HAVE_GETUID
    fprintf(stderr, ", user = %s (%d)\n", username, getuid());
#endif
    __pmServerDumpRequestPorts(stderr);
    fflush(stderr);

    if (__pmSecureServerCertificateSetup(certdb, dbpassfile, cert_nickname) < 0)
	DontStart();

    /* all the work is done here */
    ClientLoop();

    Shutdown();
    exit(0);
}

/* Convert a file descriptor to a string describing what it is for. */
static char *
FdToString(int fd)
{
    static char fdStr[FDNAMELEN];
    static char *stdFds[4] = {"*UNKNOWN FD*", "stdin", "stdout", "stderr"};
    int		i;

    if (fd >= -1 && fd < 3)
	return stdFds[fd + 1];
    if (__pmServerRequestPortString(fd, fdStr, FDNAMELEN) != NULL)
	return fdStr;
    for (i = 0; i < nClients; i++) {
	if (client[i].status.connected && fd == client[i].fd) {
	    pmsprintf(fdStr, FDNAMELEN, "client[%d] client socket", i);
	    return fdStr;
	}
	if (client[i].status.connected && fd == client[i].pmcd_fd) {
	    pmsprintf(fdStr, FDNAMELEN, "client[%d] pmcd socket", i);
	    return fdStr;
	}
    }
    return stdFds[0];
}
