/*
 * Copyright (c) 1995-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2012 Red Hat.  All Rights Reserved.
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

#define _WIN32_WINNT	0x0500	/* for CreateHardLink */
#include <math.h>
#include <ctype.h>
#include <sys/stat.h>
#include "logger.h"

#if !defined(SIGRTMAX)
#if defined(NSIG)
#define SIGRTMAX   (NSIG)
#else
! bozo neither NSIG nor SIGRTMAX are defined
#endif
#endif

/* The logger will try to allocate port numbers beginning with the number
 * defined below.  If that is in use it will keep adding one and trying again
 * until it allocates a port.
 */
#define PORT_BASE	4330	/* Base of range for port numbers */

static char	*ctlfile;	/* Control directory/portmap name */
static char	*linkfile;	/* Link name for primary logger */

int		ctlfd;		/* fd for control port */
int		ctlport;	/* pmlogger control port number */
int		wantflush;	/* flush via SIGUSR1 flag */

static void
cleanup(void)
{
    if (linkfile != NULL)
	unlink(linkfile);
    if (ctlfile != NULL)
	unlink(ctlfile);
}

static void
sigexit_handler(int sig)
{
#ifdef PCP_DEBUG
    fprintf(stderr, "pmlogger: Signalled (signal=%d), exiting\n", sig);
#endif
    cleanup();
    exit(1);
}

#ifndef IS_MINGW
static void
sigcore_handler(int sig)
{
#ifdef PCP_DEBUG
    fprintf(stderr, "pmlogger: Signalled (signal=%d), exiting (core dumped)\n", sig);
#endif
    __pmSetSignalHandler(SIGABRT, SIG_DFL);	/* Don't come back here */
    cleanup();
    abort();
}
#endif

static void
sighup_handler(int sig)
{
    /* SIGHUP is used to force a log volume change */
    __pmSetSignalHandler(SIGHUP, SIG_IGN);
    newvolume(VOL_SW_SIGHUP);
    __pmSetSignalHandler(SIGHUP, sighup_handler);
}

#ifndef IS_MINGW
static void
sigpipe_handler(int sig)
{
    /*
     * just ignore the signal, the write() will fail, and the PDU
     * xmit will return with an error
     */
    __pmSetSignalHandler(SIGPIPE, sigpipe_handler);
}
#endif

static void
sigusr1_handler(int sig)
{
    /*
     * set the flag ... flush occurs in main event loop when we're
     * quiescent
     */
    wantflush = 1;
#ifndef IS_MINGW
    __pmSetSignalHandler(SIGUSR1, sigusr1_handler);
#endif
}

#ifndef IS_MINGW
/*
 * if we are launched from pmRecord*() in libpcp, then we
 * may end up using popen() to run xconfirm(1), and then there
 * is a chance of us receiving SIGCHLD ... just ignore this signal
 */
static void
sigchld_handler(int sig)
{
}
#endif

typedef struct {
    int		sig;
    void	(*func)(int);
} sig_map_t;

/* This is used to set the dispositions for the various signals received.
 * Try to do the right thing for the various STOP/CONT signals.
 */
static sig_map_t	sig_handler[] = {
    { SIGHUP,	sighup_handler },	/* Exit   Hangup [see termio(7)] */
    { SIGINT,	sigexit_handler },	/* Exit   Interrupt [see termio(7)] */
#ifndef IS_MINGW
    { SIGQUIT,	sigcore_handler },	/* Core   Quit [see termio(7)] */
    { SIGILL,	sigcore_handler },	/* Core   Illegal Instruction */
    { SIGTRAP,	sigcore_handler },	/* Core   Trace/Breakpoint Trap */
    { SIGABRT,	sigcore_handler },	/* Core   Abort */
#ifdef SIGEMT
    { SIGEMT,	sigcore_handler },	/* Core   Emulation Trap */
#endif
    { SIGFPE,	sigcore_handler },	/* Core   Arithmetic Exception */
    { SIGKILL,	sigexit_handler },	/* Exit   Killed */
    { SIGBUS,	sigcore_handler },	/* Core   Bus Error */
    { SIGSEGV,	sigcore_handler },	/* Core   Segmentation Fault */
    { SIGSYS,	sigcore_handler },	/* Core   Bad System Call */
    { SIGPIPE,	sigpipe_handler },	/* Exit   Broken Pipe */
    { SIGALRM,	sigexit_handler },	/* Exit   Alarm Clock */
#endif
    { SIGTERM,	sigexit_handler },	/* Exit   Terminated */
    { SIGUSR1,	sigusr1_handler },	/* Exit   User Signal 1 */
#ifndef IS_MINGW
    { SIGUSR2,	sigexit_handler },	/* Exit   User Signal 2 */
    { SIGCHLD,	sigchld_handler },	/* NOP    Child stopped or terminated */
#ifdef SIGPWR
    { SIGPWR,	SIG_DFL },		/* Ignore Power Fail/Restart */
#endif
    { SIGWINCH,	SIG_DFL },		/* Ignore Window Size Change */
    { SIGURG,	SIG_DFL },		/* Ignore Urgent Socket Condition */
#ifdef SIGPOLL
    { SIGPOLL,	sigexit_handler },	/* Exit   Pollable Event [see streamio(7)] */
#endif
    { SIGSTOP,	SIG_DFL },		/* Stop   Stopped (signal) */
    { SIGTSTP,	SIG_DFL },		/* Stop   Stopped (user) */
    { SIGCONT,	SIG_DFL },		/* Ignore Continued */
    { SIGTTIN,	SIG_DFL },		/* Stop   Stopped (tty input) */
    { SIGTTOU,	SIG_DFL },		/* Stop   Stopped (tty output) */
    { SIGVTALRM, sigexit_handler },	/* Exit   Virtual Timer Expired */

    { SIGPROF,	sigexit_handler },	/* Exit   Profiling Timer Expired */
    { SIGXCPU,	sigcore_handler },	/* Core   CPU time limit exceeded [see getrlimit(2)] */
    { SIGXFSZ,	sigcore_handler}	/* Core   File size limit exceeded [see getrlimit(2)] */
#endif
};

/* Create socket for incoming connections and bind to it an address for
 * clients to use.  Only returns if it succeeds (exits on failure).
 */

static int
GetPort(char *file)
{
    int			fd;
    int			mapfd;
    FILE		*mapstream;
    int			sts;
    __pmSockAddrIn	myAddr;
    static int		port_base = -1;
    __pmHostEnt		he;
    char		*hebuf;

    fd = __pmCreateSocket();
    if (fd < 0) {
	fprintf(stderr, "GetPort: socket failed: %s\n", netstrerror());
	exit(1);
    }

    if (port_base == -1) {
	/*
	 * get optional stuff from environment ...
	 *	PMLOGGER_PORT
	 */
	char	*env_str;
	if ((env_str = getenv("PMLOGGER_PORT")) != NULL) {
	    char	*end_ptr;

	    port_base = strtol(env_str, &end_ptr, 0);
	    if (*end_ptr != '\0' || port_base < 0) {
		fprintf(stderr, 
			 "GetPort: ignored bad PMLOGGER_PORT = '%s'\n", env_str);
		port_base = PORT_BASE;
	    }
	}
	else
	    port_base = PORT_BASE;
    }

    /*
     * try to allocate ports from port_base.  If port already in use, add one
     * and try again.
     */
    for (ctlport = port_base; ; ctlport++) {
        __pmInitSockAddr(&myAddr, htonl(INADDR_ANY), htons(ctlport));
	sts = __pmBind(fd, (__pmSockAddr*)&myAddr, sizeof(myAddr));
	if (sts < 0) {
	    if (neterror() != EADDRINUSE) {
		fprintf(stderr, "bind(%d): %s\n", ctlport, netstrerror());
		exit(1);
	    }
	}
	else
	    break;
    }
    sts = __pmListen(fd, 5);	/* Max. of 5 pending connection requests */
    if (sts == -1) {
	fprintf(stderr, "__pmListen: %s\n", netstrerror());
	exit(1);
    }

    /* create and initialize the port map file */
    unlink(file);
    mapfd = open(file, O_WRONLY | O_EXCL | O_CREAT,
		 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (mapfd == -1) {
	fprintf(stderr, "%s: error creating port map file %s: %s.  Exiting.\n",
		pmProgname, file, osstrerror());
	exit(1);
    }
    /* write the port number to the port map file */
    if ((mapstream = fdopen(mapfd, "w")) == NULL) {
	perror("GetPort: fdopen");
	exit(1);
    }
    /* first the port number */
    fprintf(mapstream, "%d\n", ctlport);

    /* then the PMCD host */
    hebuf = __pmAllocHostEntBuffer();
    if (__pmGetHostByName(pmcd_host, &he, hebuf) != NULL)
        fprintf(mapstream, "%s", he.h_name);
    fprintf(mapstream, "\n");
    __pmFreeHostEntBuffer(hebuf);

    /* then the full pathname to the archive base */
    __pmNativePath(archBase);
    if (__pmAbsolutePath(archBase))
	fprintf(mapstream, "%s\n", archBase);
    else {
	char		path[MAXPATHLEN];

	if (getcwd(path, MAXPATHLEN) == NULL)
	    fprintf(mapstream, "\n");
	else
	    fprintf(mapstream, "%s%c%s\n", path, __pmPathSeparator(), archBase);
    }

    /* and finally, the annotation from -m or -x */
    if (note != NULL)
	fprintf(mapstream, "%s\n", note);

    fclose(mapstream);
    close(mapfd);

    return fd;
}

/* Create the control port for this pmlogger and the file containing the port
 * number so that other programs know which port to connect to.
 * If this is the primary pmlogger, create the special symbolic link to the
 * control file.
 */
void
init_ports(void)
{
    int		i, j, n, sts;
    int		sep = __pmPathSeparator();
    int		extlen, baselen;
    char	path[MAXPATHLEN];
    pid_t	mypid = getpid();

    /*
     * make sure control port files are removed when pmlogger terminates
     * by trapping all the signals we can
     */
    for (i = 0; i < sizeof(sig_handler)/sizeof(sig_handler[0]); i++) {
	__pmSetSignalHandler(sig_handler[i].sig, sig_handler[i].func);
    }
    /*
     * install explicit handler for other signals ... we assume all
     * of the interesting signals we are likely to receive are smaller
     * than 32 (this is a hack 'cause there is no portable way of
     * determining the maximum signal number)
     */
    for (j = 1; j < 32; j++) {
	for (i = 0; i < sizeof(sig_handler)/sizeof(sig_handler[0]); i++) {
	    if (j == sig_handler[i].sig) break;
        }
        if (i == sizeof(sig_handler)/sizeof(sig_handler[0]))
	    /* not special cased in seg_handler[] */
	    __pmSetSignalHandler(j, sigexit_handler);
    }

#if defined(HAVE_ATEXIT)
    if (atexit(cleanup) != 0) {
	perror("atexit");
	fprintf(stderr, "%s: unable to register atexit cleanup function.  Exiting\n",
		pmProgname);
	cleanup();
	exit(1);
    }
#endif

    /* create the control port file (make the directory if necessary). */

    /* count digits in mypid */
    for (n = mypid, extlen = 1; n ; extlen++)
	n /= 10;
    /* baselen is directory + trailing / */
    snprintf(path, sizeof(path), "%s%cpmlogger", pmGetConfig("PCP_TMP_DIR"), sep);
    baselen = strlen(path) + 1;
    /* likewise for PCP_DIR if it is set */
    n = baselen + extlen + 1;
    ctlfile = (char *)malloc(n);
    if (ctlfile == NULL)
	__pmNoMem("port file name", n, PM_FATAL_ERR);
    strcpy(ctlfile, path);

    /* try to create the port file directory. OK if it already exists */
    sts = mkdir2(ctlfile, S_IRWXU | S_IRWXG | S_IRWXO);
    if (sts < 0 && oserror() != EEXIST) {
	fprintf(stderr, "%s: error creating port file dir %s: %s\n",
		pmProgname, ctlfile, osstrerror());
	exit(1);
    }
    chmod(ctlfile, S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX);

    /* remove any existing port file with my name (it's old) */
    snprintf(ctlfile + (baselen-1), n, "%c%" FMT_PID, sep, mypid);
    sts = unlink(ctlfile);
    if (sts == -1 && oserror() != ENOENT) {
	fprintf(stderr, "%s: error removing %s: %s.  Exiting.\n",
		pmProgname, ctlfile, osstrerror());
	exit(1);
    }

    /* get control port and write port map file */
    ctlfd = GetPort(ctlfile);

    /*
     * If this is the primary logger, make the special symbolic link for
     * clients to connect specifically to it.
     */
    if (primary) {
	baselen = snprintf(path, sizeof(path), "%s%cpmlogger",
				pmGetConfig("PCP_TMP_DIR"), sep);
	n = baselen + 9;	/* separator + "primary" + null */
	linkfile = (char *)malloc(n);
	if (linkfile == NULL)
	    __pmNoMem("primary logger link file name", n, PM_FATAL_ERR);
	snprintf(linkfile, n, "%s%cprimary", path, sep);
#ifndef IS_MINGW
	sts = symlink(ctlfile, linkfile);
#else
	sts = (CreateHardLink(linkfile, ctlfile, NULL) == 0);
#endif
	if (sts != 0) {
	    if (oserror() == EEXIST)
		fprintf(stderr, "%s: there is already a primary pmlogger running\n", pmProgname);
	    else
		fprintf(stderr, "%s: error creating primary logger link %s: %s\n",
			pmProgname, linkfile, osstrerror());
	    exit(1);
	}
    }
}

/* Service a request on the control port  Return non-zero if a new client
 * connection has been accepted.
 */

int		clientfd = -1;
unsigned int	clientops = 0;		/* for access control (deny ops) */
char		pmlc_host[MAXHOSTNAMELEN];
int		connect_state = 0;

int
control_req(void)
{
    int			fd, sts;
    __pmSockAddrIn	addr;
    __pmHostEnt		h;
    char		*hbuf, *abuf;
    __pmSockLen		addrlen;

    addrlen = sizeof(addr);
    fd = __pmAccept(ctlfd, (__pmSockAddr *)&addr, &addrlen);
    if (fd == -1) {
	fprintf(stderr, "error accepting client: %s\n", netstrerror());
	return 0;
    }
    __pmSetSocketIPC(fd);
    if (clientfd != -1) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "control_req: send EADDRINUSE on fd=%d (client already on fd=%d)\n", fd, clientfd);
#endif
	sts = __pmSendError(fd, FROM_ANON, -EADDRINUSE);
	if (sts < 0)
	    fprintf(stderr, "error sending connection NACK to client: %s\n",
			 pmErrStr(sts));
	__pmCloseSocket(fd);
	return 0;
    }

    sts = __pmSetVersionIPC(fd, UNKNOWN_VERSION);
    if (sts < 0) {
	__pmSendError(fd, FROM_ANON, sts);
	fprintf(stderr, "error connecting to client: %s\n", pmErrStr(sts));
	__pmCloseSocket(fd);
	return 0;
    }

    hbuf = __pmAllocHostEntBuffer();
    if (__pmGetHostByAddr(& addr, &h, hbuf) == NULL || strlen(h.h_name) > MAXHOSTNAMELEN-1) {
	abuf = __pmSockAddrInToString(&addr);
        sprintf(pmlc_host, "%s", abuf);
	free(abuf);
    }
    else
	/* this is safe, due to strlen() test above */
	strcpy(pmlc_host, h.h_name);
    __pmFreeHostEntBuffer(hbuf);

    if ((sts = __pmAccAddClient(__pmSockAddrInToIPAddr(&addr), &clientops)) < 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    abuf = __pmSockAddrInToString(&addr);
	    fprintf(stderr, "client addr: %s\n", abuf);
	    free(abuf);
	    __pmAccDumpHosts(stderr);
	    fprintf(stderr, "control_req: connection rejected on fd=%d from %s: %s\n", fd, pmlc_host, pmErrStr(sts));
	}
#endif
	sts = __pmSendError(fd, FROM_ANON, sts);
	if (sts < 0)
	    fprintf(stderr, "error sending connection access NACK to client: %s\n",
			 pmErrStr(sts));
	sleep(1);	/* QA 083 seems like there is a race w/out this delay */
	__pmCloseSocket(fd);
	return 0;
    }

    /*
     * encode pdu version in the acknowledgement
     * also need "from" to be pmlogger's pid as this is checked at
     * the other end
     */
    sts = __pmSendError(fd, (int)getpid(), LOG_PDU_VERSION);
    if (sts < 0) {
	fprintf(stderr, "error sending connection ACK to client: %s\n",
		     pmErrStr(sts));
	__pmCloseSocket(fd);
	return 0;
    }
    clientfd = fd;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "control_req: connection accepted on fd=%d from %s\n", fd, pmlc_host);
#endif

    return 1;
}
