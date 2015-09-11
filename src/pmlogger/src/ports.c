/*
 * Copyright (c) 2012-2015 Red Hat.
 * Copyright (c) 1995-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <math.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
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
static const char *socketPath;	/* Path to unix domain sockets. */
static const char *linkSocketPath;/* Link to socket for primary logger */

int		ctlfds[CFD_NUM] = {-1, -1, -1};/* fds for control ports: */
int		ctlport;	/* pmlogger control port number */

static void
cleanup(void)
{
    /*
     * flush all stdio buffers, _then_ remove the control files ...
     * we have QA than camps on the control file(s) and assumes the
     * log file is complete once the control file(s) is removed.
     */
    fflush(NULL);

    if (linkfile != NULL)
	unlink(linkfile);
    if (ctlfile != NULL)
	unlink(ctlfile);
    if (linkSocketPath != NULL)
	unlink(linkSocketPath);
    if (socketPath != NULL)
	unlink(socketPath);
}

static void
sigexit_handler(int sig)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_DESPERATE)
	fprintf(stderr, "pmlogger: Signalled (signal=%d), exiting\n", sig);
#endif
    cleanup();
    _exit(sig);
}

static void
sigterm_handler(int sig)
{
    /* exit as soon as possible, handler is deferred for log cleanup */
    exit_code = sig;
}

static void
sighup_handler(int sig)
{
    __pmSetSignalHandler(SIGHUP, sighup_handler);
    vol_switch_flag = 1;
}

#ifndef IS_MINGW
static void
sigcore_handler(int sig)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_DESPERATE)
	fprintf(stderr, "pmlogger: Signalled (signal=%d), exiting (core dumped)\n", sig);
#endif
    __pmSetSignalHandler(SIGABRT, SIG_DFL);	/* Don't come back here */
    cleanup();
    _exit(sig);
}

static void
sigpipe_handler(int sig)
{
    /*
     * just ignore the signal, the write() will fail, and the PDU
     * xmit will return with an error
     */
    __pmSetSignalHandler(SIGPIPE, sigpipe_handler);
}

static void
sigusr1_handler(int sig)
{
    /*
     * no-op now that all archive write I/O is unbuffered
     */
    __pmSetSignalHandler(SIGUSR1, sigusr1_handler);
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
    { SIGINT,	sigterm_handler },	/* Exit   Interrupt [see termio(7)] */
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
    { SIGALRM,	sigterm_handler },	/* Exit   Alarm Clock */
#endif
    { SIGTERM,	sigterm_handler },	/* Exit   Terminated */
#ifndef IS_MINGW
    { SIGUSR1,	sigusr1_handler },	/* NOP    User Signal 1 - [was fflush(3)] */
    { SIGUSR2,	sigexit_handler },	/* Exit   User Signal 2 */
    { SIGCHLD,	SIG_IGN },		/* NOP    Child stopped or terminated */
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
    { SIGVTALRM, sigterm_handler },	/* Exit   Virtual Timer Expired */

    { SIGPROF,	sigterm_handler },	/* Exit   Profiling Timer Expired */
    { SIGXCPU,	sigcore_handler },	/* Core   CPU time limit exceeded [see getrlimit(2)] */
    { SIGXFSZ,	sigcore_handler}	/* Core   File size limit exceeded [see getrlimit(2)] */
#endif
};

/* Create a network socket for incoming connections and bind to it an address for
 * clients to use.
 * If supported, also create a unix domain socket for local clients to use.
 * Only returns if it succeeds (exits on failure).
 */

static void
GetPorts(char *file)
{
    int			fd;
    int			mapfd = -1;
    FILE		*mapstream = NULL;
    int			socketsCreated = 0;
    int			maxpending = 5;	/* Max. pending connection requests */
    int			address = INADDR_ANY;
    int			ctlix;
    int			sts;
    char		*env_str;
    __pmSockAddr	*myAddr;
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    char		globalPath[MAXPATHLEN];
    char		localPath[MAXPATHLEN];
#endif
    static int		port_base = -1;

    if ((env_str = getenv("PMLOGGER_MAXPENDING")) != NULL)
	maxpending = atoi(env_str);

    /* Try to create sockets for control connections. */
    for (ctlix = 0; ctlix < CFD_NUM; ++ctlix) {
	if (ctlix == CFD_UNIX) {
#if defined(HAVE_STRUCT_SOCKADDR_UN)
	    const char *socketError;
	    const char *errorPath;
	    /* Try to create a unix domain socket, if supported. */
	    fd = __pmCreateUnixSocket();
	    if (fd < 0) {
		fprintf(stderr, "GetPorts: unix domain socket failed: %s\n", netstrerror());
		continue;
	    }
	    if ((myAddr = __pmSockAddrAlloc()) == NULL) {
		fprintf(stderr, "GetPorts: __pmSockAddrAlloc out of memory\n");
		exit(1);
	    }
	    socketPath = __pmLogLocalSocketDefault(getpid(), globalPath, sizeof(globalPath));
	    __pmSockAddrSetFamily(myAddr, AF_UNIX);
	    __pmSockAddrSetPath(myAddr, socketPath);
	    __pmServerSetLocalSocket(socketPath);
	    sts = __pmBind(fd, (void *)myAddr, __pmSockAddrSize());

	    /*
	     * If we cannot bind to the system wide socket path, then try binding
	     * to the user specific one.
	     */
	    if (sts < 0) {
		char *tmpPath;
		socketError = netstrerror();
		errorPath = socketPath;
		unlink(errorPath);
		socketPath = __pmLogLocalSocketUser(getpid(), localPath, sizeof(localPath));
		if (socketPath == NULL) {
		    sts = -ESRCH;
		}
		else {
		    /*
		     * Make sure that the directory exists. dirname may modify the
		     * contents of its first argument, so use a copy.
		     */
		    if ((tmpPath = strdup(socketPath)) == NULL) {
			fprintf(stderr, "GetPorts: strdup out of memory\n");
			exit(1);
		    }
		    sts = __pmMakePath(dirname(tmpPath),
				       S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		    free(tmpPath);
		    if (sts >= 0 || oserror() == EEXIST) {
			__pmSockAddrSetPath(myAddr, socketPath);
			__pmServerSetLocalSocket(socketPath);
			sts = __pmBind(fd, (void *)myAddr, __pmSockAddrSize());
		    }
		}
	    }
	    __pmSockAddrFree(myAddr);

	    if (sts < 0) {
		/* Could not bind to either socket path. */
		fprintf(stderr, "__pmBind(%s): %s\n", errorPath, socketError);
		if (sts == -ESRCH)
		    fprintf(stderr, "__pmLogLocalSocketUser(): %s\n", osstrerror());
		else
		    fprintf(stderr, "__pmBind(%s): %s\n", socketPath, netstrerror());
	    }
	    else {
		/*
		 * For unix domain sockets, grant rw access to the socket for all,
		 * otherwise, on linux platforms, connection will not be possible.
		 * This must be done AFTER binding the address. See Unix(7) for details.
		 */
		sts = chmod(socketPath, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (sts != 0) {
		    fprintf(stderr, "GetPorts: chmod(%s): %s\n", socketPath, strerror(errno));
		}
	    }
	    /* On error, don't leave the socket file lying around. */
	    if (sts < 0) {
		unlink(socketPath);
		socketPath = NULL;
	    }
	    else if ((socketPath = strdup(socketPath)) == NULL) {
		fprintf(stderr, "GetPorts: strdup out of memory\n");
		exit(1);
	    }
#else
	    /*
	     * Unix domain sockets are not supported.
	     * This is not an error, just don't try to create one.
	     */
	    continue;
#endif
	}
	else {
	    /* Try to create a network socket. */
	    if (ctlix == CFD_INET) {
		fd = __pmCreateSocket();
		if (fd < 0) {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_CONTEXT)
			fprintf(stderr, "GetPorts: inet socket creation failed: %s\n",
			    netstrerror());
#endif
		    continue;
		}
	    }
	    else {
		fd = __pmCreateIPv6Socket();
		if (fd < 0) {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_CONTEXT)
			fprintf(stderr, "GetPorts: ipv6 socket creation failed: %s\n",
			    netstrerror());
#endif
		    continue;
		}
	    }
	    if (port_base == -1) {
		/*
		 * get optional stuff from environment ...
		 *	PMLOGGER_PORT,
		 *	PMLOGGER_LOCAL
		 */
		if ((env_str = getenv("PMLOGGER_LOCAL")) != NULL) {
		    if (atoi(env_str) != 0)
			address = INADDR_LOOPBACK;
		}
		if ((env_str = getenv("PMLOGGER_PORT")) != NULL) {
		    char	*end_ptr;

		    port_base = strtol(env_str, &end_ptr, 0);
		    if (*end_ptr != '\0' || port_base < 0) {
			fprintf(stderr, 
				"GetPorts: ignored bad PMLOGGER_PORT = '%s'\n", env_str);
			port_base = PORT_BASE;
		    }
		}
		else
		    port_base = PORT_BASE;
	    }

	    /*
	     * try to allocate ports from port_base.  If port already in use,
	     * add one and try again.
	     */
	    if ((myAddr = __pmSockAddrAlloc()) == NULL) {
		fprintf(stderr, "GetPorts: __pmSockAddrAlloc out of memory\n");
		exit(1);
	    }
	    for (ctlport = port_base; ; ctlport++) {
		if (ctlix == CFD_INET)
		    __pmSockAddrInit(myAddr, AF_INET, address, ctlport);
		else
		    __pmSockAddrInit(myAddr, AF_INET6, address, ctlport);
		sts = __pmBind(fd, (void *)myAddr, __pmSockAddrSize());
		if (sts < 0) {
		    if (neterror() != EADDRINUSE) {
			fprintf(stderr, "__pmBind(%d): %s\n", ctlport, netstrerror());
			break;
		    }
		    if (address == INADDR_LOOPBACK)
			break;
		}
		else
		    break;
	    }
	    __pmSockAddrFree(myAddr);
	}

	/* Now listen on the new socket. */
	if (sts >= 0) {
	    sts = __pmListen(fd, maxpending);
	    if (sts == -1) {
		__pmCloseSocket(fd);
		fprintf(stderr, "__pmListen: %s\n", netstrerror());
	    }
	    else {
		ctlfds[ctlix] = fd;
		++socketsCreated;
	    }
	}
    }

    if (socketsCreated != 0) {
	/* create and initialize the port map file */
	unlink(file);
	mapfd = open(file, O_WRONLY | O_EXCL | O_CREAT,
		     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (mapfd == -1) {
	    /* not a fatal error; continue on without control file */
#ifdef DESPERATE
	    fprintf(stderr, "%s: error creating port map file %s: %s.  Exiting.\n",
		    pmProgname, file, osstrerror());
#endif
	    return;
	}
	/* write the port number to the port map file */
	if ((mapstream = fdopen(mapfd, "w")) == NULL) {
	    /* not a fatal error; continue on without control file */
	    close(mapfd);
#ifdef DESPERATE
	    perror("GetPorts: fdopen");
#endif
	    return;
	}
	/* first the port number */
	fprintf(mapstream, "%d\n", ctlport);

	/* then the PMCD host (but don't bother try DNS-canonicalize) */
	fprintf(mapstream, "%s\n", pmcd_host);

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
    }

    if (mapstream != NULL)
	fclose(mapstream);
    if (mapfd >= 0)
	close(mapfd);
    if (socketsCreated == 0)
	exit(1);
}

/* Create the control port for this pmlogger and the file containing the port
 * number so that other programs know which port to connect to.
 * If this is the primary pmlogger, create the special link to the
 * control file.
 */
void
init_ports(void)
{
    int		i, j, n, sts;
    int		sep = __pmPathSeparator();
    int		extlen, baselen;
    char	path[MAXPATHLEN];
    char	pidfile[MAXPATHLEN];
    int		pidlen;
    struct stat	sbuf;
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
    if (sts < 0) {
	if (oserror() != EEXIST) {
	    fprintf(stderr, "%s: error creating port file dir %s: %s\n",
		pmProgname, ctlfile, osstrerror());
	    exit(1);
	}
    } else {
	chmod(ctlfile, S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX);
    }

    /* remove any existing port file with my name (it's old) */
    snprintf(ctlfile + (baselen-1), n, "%c%" FMT_PID, sep, mypid);
    unlink(ctlfile);

    /* get control port and write port map file */
    GetPorts(ctlfile);

    /*
     * If this is the primary logger, make the special link for
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
	/*
	 * Remove legacy linkfile (i.e. if it exists and is NOT a symlink).
	 * This can occur after an upgrade if pmlogger was SIGKILL'ed, but
	 * normally an upgrade will restart pmlogger and atexit(cleanup) will
	 * have been run, which will remove legacy hardlinks.
	 */
	if (stat(linkfile, &sbuf) == 0 && !S_ISLNK(sbuf.st_mode)) {
	    if (unlink(linkfile) != 0) {
		fprintf(stderr, "%s: warning: failed to remove '%s' hardlink to stale control file '%s': %s\n",
			pmProgname, linkfile, pidfile, osstrerror());
	    }
#ifdef PCP_DEBUG
	    else if (pmDebug & DBG_TRACE_CONTEXT) {
		fprintf(stderr, "%s: info: removed '%s' old-style hardlink to stale control file '%s': %s\n",
			pmProgname, linkfile, pidfile, osstrerror());
	    }
#endif
	}
#endif

	/*
	 * Remove symlink if it is stale (i.e. exists but the process does not).
	 */
	if ((pidlen = readlink(linkfile, pidfile, sizeof(pidfile))) > 0) {
	    /* control file name is the PID of the primary pmlogger process */
	    pidfile[pidlen] = '\0';
	    pid_t pid = atoi(pidfile);
	    if (!__pmProcessExists(pid)) {
	    	if (unlink(linkfile) != 0) {
		    fprintf(stderr, "%s: warning: failed to remove '%s' symlink to stale control file '%s': %s\n",
			    pmProgname, linkfile, pidfile, osstrerror());
		}
#ifdef PCP_DEBUG
		else if (pmDebug & DBG_TRACE_CONTEXT) {
		    fprintf(stderr, "%s: info: removed '%s' symlink to stale control file '%s': %s\n",
			    pmProgname, linkfile, pidfile, osstrerror());
		}
#endif
		/* remove the stale control file too */
	    	if (unlink(pidfile) != 0) {
		    fprintf(stderr, "%s: warning: failed to remove stale control file '%s': %s\n",
			    pmProgname, pidfile, osstrerror());
		}
#ifdef PCP_DEBUG
		else if (pmDebug & DBG_TRACE_CONTEXT) {
		    fprintf(stderr, "%s: info: removed stale control file '%s': %s\n",
			    pmProgname, pidfile, osstrerror());
		}
#endif
	    }
	}

	/*
	 * If the symlink still exists then there really is another primary logger running
	 */
	if (access(linkfile, F_OK) == 0) {
	    /* configuration error - only one primary pmlogger should be configured */
	    fprintf(stderr, "%s: ERROR: there is already a primary pmlogger running, ctlfile=%s linkfile=%s\n",
		    pmProgname, ctlfile, linkfile);
	    exit(1);
	}

	if ((sts = symlink(ctlfile, linkfile)) != 0) {
	    fprintf(stderr, "%s: error creating primary logger symbolic link %s: %s\n",
		    pmProgname, linkfile, osstrerror());
	}
#ifdef PCP_DEBUG
	else if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "%s: info: created control file symlink %s -> %s\n", pmProgname, linkfile, ctlfile);
	}
#endif

#if defined(HAVE_STRUCT_SOCKADDR_UN)
	/*
	 * Create a symbolic link to the local socket for users wanting the primary logger.
	 */
	linkSocketPath = __pmLogLocalSocketDefault(PM_LOG_PRIMARY_PID, path, sizeof(path));

	/*
	 * Remove legacy linkSocketPath hardlink (i.e. if it is a socket).
	 * This can occur after an upgrade, similarly to the control file link,
	 * see above.
	 */
	if (stat(linkSocketPath, &sbuf) == 0 && S_ISSOCK(sbuf.st_mode)) {
	    if (unlink(linkSocketPath) != 0) {
		fprintf(stderr, "%s: warning: failed to remove '%s' hardlink to stale socket '%s': %s\n",
			pmProgname, linkSocketPath, pidfile, osstrerror());
	    }
#ifdef PCP_DEBUG
	    else if (pmDebug & DBG_TRACE_CONTEXT) {
		fprintf(stderr, "%s: info: removed '%s' old-style hardlink to stale socket '%s': %s\n",
			pmProgname, linkSocketPath, pidfile, osstrerror());
	    }
#endif
	}

	/* Remove the symlink if it points to a stale primary pmlogger socket */
	if ((pidlen = readlink(linkSocketPath, pidfile, sizeof(pidfile))) > 0) {
	    pidfile[pidlen] = '\0';
	    for (i=0; i < pidlen; i++) {
		/* first digit is the start of the PID */
		if (isdigit(pidfile[i])) {
		    pid_t pid = atoi(pidfile + i);
		    if (!__pmProcessExists(pid)) {
			if (unlink(linkSocketPath) != 0) {
			    fprintf(stderr, "%s: warning: failed to remove '%s' symlink to stale socket '%s': %s\n",
				    pmProgname, linkSocketPath, pidfile, osstrerror());
			}
#ifdef PCP_DEBUG
			else if (pmDebug & DBG_TRACE_CONTEXT) {
			    fprintf(stderr, "%s: info: removed '%s' symlink to stale socket '%s'\n",
				    pmProgname, linkSocketPath, pidfile);
			}
#endif
			/* remove the stale socket too */
			if (unlink(pidfile) != 0) {
			    fprintf(stderr, "%s: warning: failed to remove stale pmlogger socket '%s': %s\n",
				    pmProgname, pidfile, osstrerror());
			}
#ifdef PCP_DEBUG
			else if (pmDebug & DBG_TRACE_CONTEXT) {
			    fprintf(stderr, "%s: info: removed stale pmlogger socket '%s'\n",
				    pmProgname, pidfile);
			}
#endif
		    }
		    break;
		}
	    }
	}

	/*
	 * As above, if the symlink still exists then there really is
	 * another primary logger running - we shouldn't get to here
	 * but maybe someone manually deleted the primary->control link.
	 */
	if (access(linkSocketPath, F_OK) == 0) {
	    /* configuration error - only one primary pmlogger should be configured */
	    fprintf(stderr, "%s: ERROR: there is already a primary pmlogger running, socketPath=%s linkSocketPath=%s\n",
		    pmProgname, socketPath, linkSocketPath);
	    exit(1);
	}

	/*
	 * Create the symlink to the primary pmlogger control socket.
	 */
	if ((sts = symlink(socketPath, linkSocketPath)) != 0) {
	    fprintf(stderr, "%s: error creating primary logger socket symbolic link %s: %s\n",
		    pmProgname, linkSocketPath, osstrerror());
	}
#ifdef PCP_DEBUG
	else if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "%s: info: created primary pmlogger socket symlink %s -> %s\n",
		    pmProgname, linkSocketPath, socketPath);
	}
#endif

	if ((linkSocketPath = strdup(linkSocketPath)) == NULL) {
	    fprintf(stderr, "init_ports: strdup out of memory\n");
	    exit(1);
	}
#endif
    }
}

/* Service a request on the control port  Return non-zero if a new client
 * connection has been accepted.
 */

int		clientfd = -1;
unsigned int	denyops = 0;		/* for access control (ops not allowed) */
char		pmlc_host[MAXHOSTNAMELEN];
int		connect_state = 0;

#if defined(HAVE_STRUCT_SOCKADDR_UN)
static int
check_local_creds(__pmHashCtl *attrs)
{
    __pmHashNode	*node;
    const char		*connectingUser;
    char		*end;
    __pmUserID		connectingUid;

    /* Get the user name of the connecting process. */
    connectingUser = ((node = __pmHashSearch(PCP_ATTR_USERID, attrs)) ?
			(const char *)node->data : NULL);
    if (connectingUser == NULL) {
	/* We don't know who is connecting. */
	return PM_ERR_PERMISSION;
    }

    /* Get the uid of the connecting process. */
    errno = 0;
    connectingUid = strtol(connectingUser, &end, 0);
    if (errno != 0 || *end != '\0') {
	/* Can't convert the connecting user to a uid cleanly. */
	return PM_ERR_PERMISSION;
    }

    /* Allow connections from root (uid == 0). */
    if (connectingUid == 0)
	return 0;

    /* Allow connections from the same user as us. */
    if (connectingUid == getuid() || connectingUid == geteuid())
	return 0;

    /* Connection is not allowed. */
    return PM_ERR_PERMISSION;
}
#endif /* defined(HAVE_STRUCT_SOCKADDR_UN) */

int
control_req(int ctlfd)
{
    int			fd, sts;
    char		*abuf;
    char		*hostName;
    __pmSockAddr	*addr;
    __pmSockLen		addrlen;

    if ((addr = __pmSockAddrAlloc()) == NULL) {
	fputs("error allocating space for client sockaddr\n", stderr);
	return 0;
    }
    addrlen = __pmSockAddrSize();
    fd = __pmAccept(ctlfd, addr, &addrlen);
    if (fd == -1) {
	fprintf(stderr, "error accepting client: %s\n", netstrerror());
	__pmSockAddrFree(addr);
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
	__pmSockAddrFree(addr);
	__pmCloseSocket(fd);
	return 0;
    }

    sts = __pmSetVersionIPC(fd, UNKNOWN_VERSION);
    if (sts < 0) {
	__pmSendError(fd, FROM_ANON, sts);
	fprintf(stderr, "error connecting to client: %s\n", pmErrStr(sts));
	__pmSockAddrFree(addr);
	__pmCloseSocket(fd);
	return 0;
    }

    hostName = __pmGetNameInfo(addr);
    if (hostName == NULL || strlen(hostName) > MAXHOSTNAMELEN-1) {
	abuf = __pmSockAddrToString(addr);
        snprintf(pmlc_host, sizeof(pmlc_host), "%s", abuf);
	free(abuf);
    }
    else {
	/* this is safe, due to strlen() test above */
	strcpy(pmlc_host, hostName);
    }
    if (hostName != NULL)
	free(hostName);

    sts = __pmAccAddClient(addr, &denyops);
    if (sts < 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    abuf = __pmSockAddrToString(addr);
	    fprintf(stderr, "client addr: %s\n\n", abuf);
	    free(abuf);
	    __pmAccDumpHosts(stderr);
	    fprintf(stderr, "\ncontrol_req: connection rejected on fd=%d from %s: %s\n", fd, pmlc_host, pmErrStr(sts));
	}
#endif
	sts = __pmSendError(fd, FROM_ANON, sts);
	if (sts < 0)
	    fprintf(stderr, "error sending connection access NACK to client: %s\n",
			 pmErrStr(sts));
	sleep(1);	/* QA 083 seems like there is a race w/out this delay */
	__pmSockAddrFree(addr);
	__pmCloseSocket(fd);
	return 0;
    }

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    /*
     * For connections on an AF_UNIX socket, check the user credentials of the
     * connecting process.
     */
    if (__pmSockAddrGetFamily(addr) == AF_UNIX) {
	__pmHashCtl clientattrs; /* Connection attributes (auth info) */
	__pmHashInit(&clientattrs);

	/* Get the user credentials. */
	if ((sts = __pmServerSetLocalCreds(fd, &clientattrs)) < 0) {
	    sts = __pmSendError(fd, FROM_ANON, sts);
	    if (sts < 0)
		fprintf(stderr, "error sending connection credentials NACK to client: %s\n",
			pmErrStr(sts));
	    __pmSockAddrFree(addr);
	    __pmCloseSocket(fd);
	    return 0;
	}

	/* Check the user credentials. */
	if ((sts = check_local_creds(&clientattrs)) < 0) {
	    sts = __pmSendError(fd, FROM_ANON, sts);
	    if (sts < 0)
		fprintf(stderr, "error sending connection credentials NACK to client: %s\n",
			pmErrStr(sts));
	    __pmSockAddrFree(addr);
	    __pmCloseSocket(fd);
	    return 0;
	}

	/* This information is no longer needed. */
	__pmFreeAttrsSpec(&clientattrs);
	__pmHashClear(&clientattrs);
    }
#endif

    /* Done with this address. */
    __pmSockAddrFree(addr);

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
