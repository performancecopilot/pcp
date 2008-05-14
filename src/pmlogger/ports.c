/*
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "pmapi.h"
#include "impl.h"
#include "./logger.h"

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
#define PORT_BASE	4330		/* Base of range for port numbers */

static char	*ctlfile = NULL;	/* Control directory/portmap name */
static char	*linkfile = NULL;	/* Link name for primary logger */

int		ctlfd;			/* fd for control port */
int		ctlport;		/* pmlogger control port number */
int		wantflush = 0;		/* flush via SIGUSR1 flag */

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

static void
sigcore_handler(int sig)
{
#ifdef PCP_DEBUG
    fprintf(stderr, "pmlogger: Signalled (signal=%d), exiting (core dumped)\n", sig);
#endif
    cleanup();
    signal(SIGABRT, SIG_DFL);		/* Don't come back here on SIGABRT */
    abort();
}

static void
sighup_handler(int sig)
{
    /* SIGHUP is used to force a log volume change */
    signal(SIGHUP, SIG_IGN);
    newvolume(VOL_SW_SIGHUP);
    signal(SIGHUP, sighup_handler);
}

static void
sigpipe_handler(int sig)
{
    /*
     * just ignore the signal, the write() will fail, and the PDU
     * xmit will return with an error
     */
    signal(SIGPIPE, sigpipe_handler);
}

static void
sigusr1_handler(int sig)
{
    /* set the flag ... flush occurs in x */
    wantflush = 1;
    signal(SIGUSR1, sigusr1_handler);
}


/*
 * if we are launched from pmRecord*() in libpcp, then we
 * may end up using popen() to run xconfirm(1), and then there
 * is a chance of us receiving SIGCHLD ... just ignore this signal
 */
static void
sigchld_handler(int sig)
{
}

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
    { SIGTERM,	sigexit_handler },	/* Exit   Terminated */
    { SIGUSR1,	sigusr1_handler },	/* Exit   User Signal 1 */
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
    int			i, sts;
    struct sockaddr_in	myAddr;
    struct linger	noLinger = {1, 0};
    static int		port_base = -1;
    struct hostent	*hep;
    extern char	    	*archBase;		/* base name for log files */
    extern char		*pmcd_host;		/* collecting from PMCD on this host */

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	perror("socket");
	exit(1);
    }
    i = 0;	/* for purify! */
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &i,
		   (mysocklen_t)sizeof(i)) < 0) {
	perror("setsockopt(nodelay)");
	exit(1);
    }
    /* Don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &noLinger, (mysocklen_t)sizeof(noLinger)) < 0) {
	perror("setsockopt(nolinger)");
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
	memset(&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myAddr.sin_port = htons(ctlport);
	sts = bind(fd, (struct sockaddr*)&myAddr, sizeof(myAddr));
	if (sts < 0) {
	    if (errno != EADDRINUSE) {
		fprintf(stderr, "bind(%d): %s\n", ctlport, strerror(errno));
		exit(1);
	    }
	}
	else
	    break;
    }
    sts = listen(fd, 5);	/* Max. of 5 pending connection requests */
    if (sts == -1) {
	perror("listen");
	exit(1);
    }

    /* create and initialize the port map file */
    unlink(file);
    mapfd = open(file, O_WRONLY | O_EXCL | O_CREAT,
		 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (mapfd == -1) {
	fprintf(stderr, "%s: error creating port map file %s: %s.  Exiting.\n",
		pmProgname, file, strerror(errno));
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
    hep = gethostbyname(pmcd_host);
    fprintf(mapstream, "%s\n", hep == NULL ? "" : hep->h_name);

    /* and finally the full pathname to the archive base */
    if (*archBase == '/')
	fprintf(mapstream, "%s\n", archBase);
    else {
	char		path[MAXPATHLEN];
	if (getcwd(path, MAXPATHLEN) == NULL)
	    fprintf(mapstream, "\n");
	else
	    fprintf(mapstream, "%s/%s\n", path, archBase);
    }


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
    int		i, n, sts;
    int		j;
    int		extlen, baselen;
    pid_t	mypid = getpid();
    extern int	primary;		/* Non-zero for primary logger */

    /*
     * make sure control port files are removed when pmlogger terminates
     * by trapping all the signals we can
     */
    for (i = 0; i < sizeof(sig_handler)/sizeof(sig_handler[0]); i++) {
	signal(sig_handler[i].sig, sig_handler[i].func);
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
	    signal(j, sigexit_handler);
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
    baselen = strlen(PM_LOG_PORT_DIR) + 1;
    n = baselen + extlen + 1;
    ctlfile = (char *)malloc(n);
    if (ctlfile == NULL) {
	__pmNoMem("port file name", n, PM_FATAL_ERR);
    }
    strcpy(ctlfile, PM_LOG_PORT_DIR);
    
    /* try to create the port file directory. OK if it already exists */
    sts = mkdir(ctlfile, S_IRWXU | S_IRWXG | S_IRWXO);
    if (sts < 0 && errno != EEXIST) {
	fprintf(stderr, "%s: error creating port file dir %s: %s\n",
		pmProgname, ctlfile, strerror(errno));
	exit(1);
    }
    chmod(ctlfile, S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX);

    /* remove any existing port file with my name (it's old) */
    strcat(ctlfile, "/");
    sprintf(ctlfile + baselen, "%d", (int)mypid);
    sts = unlink(ctlfile);
    if (sts == -1 && errno != ENOENT) {
	fprintf(stderr, "%s: error removing %s: %s.  Exiting.\n",
		pmProgname, ctlfile, strerror(errno));
	exit(1);
    }

    /* get control port and write port map file */
    ctlfd = GetPort(ctlfile);

    /*
     * If this is the primary logger, make the special symbolic link for
     * clients to connect specifically to it.
     */
    if (primary) {
	extlen = strlen(PM_LOG_PRIMARY_LINK);
	n = baselen + extlen + 1;
	linkfile = (char *)malloc(n);
	if (linkfile == NULL) {
	    __pmNoMem("primary logger link file name", n, PM_FATAL_ERR);
	}
	strcpy(linkfile, PM_LOG_PORT_DIR);
	strcat(linkfile, "/");
	strcat(linkfile, PM_LOG_PRIMARY_LINK);
	if (symlink(ctlfile, linkfile) != 0) {
	    if (errno == EEXIST)
		fprintf(stderr, "%s: there is already a primary pmlogger running\n", pmProgname);
	    else
		fprintf(stderr, "%s: error creating primary logger link %s: %s\n",
			pmProgname, linkfile, strerror(errno));
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
    struct sockaddr_in	addr;
    struct hostent	*hp;
    mysocklen_t		addrlen;
    __pmIPC		ipc = { UNKNOWN_VERSION, NULL };

    addrlen = sizeof(addr);
    fd = accept(ctlfd, (struct sockaddr *)&addr, &addrlen);
    if (fd == -1) {
	perror("error accepting client");
	return 0;
    }
    if (clientfd != -1) {
	sts = __pmSendError(fd, PDU_BINARY, -EADDRINUSE);
	if (sts < 0)
	    fprintf(stderr, "error sending connection NACK to client: %s\n",
			 pmErrStr(sts));
	__pmResetIPC(fd);
	close(fd);
	return 0;
    }

    if ((sts = __pmAddIPC(fd, ipc)) < 0) {
	__pmSendError(fd, PDU_BINARY, sts);
	fprintf(stderr, "error connecting to client: %s\n", pmErrStr(sts));
	close(fd);
	return 0;
    }

    hp = gethostbyaddr((void *)&addr.sin_addr.s_addr, sizeof(addr.sin_addr.s_addr), AF_INET);
    if (hp == NULL || strlen(hp->h_name) > MAXHOSTNAMELEN-1) {
	char	*p = (char *)&addr.sin_addr.s_addr;

	sprintf(pmlc_host, "%d.%d.%d.%d",
		p[0] & 0xff, p[1] & 0xff, p[2] & 0xff, p[3] & 0xff);
    }
    else
	/* this is safe, due to strlen() test above */
	strcpy(pmlc_host, hp->h_name);

    if ((sts = __pmAccAddClient(&addr.sin_addr, &clientops)) < 0) {
	if (sts == PM_ERR_CONNLIMIT || sts == PM_ERR_PERMISSION)
	    sts = XLATE_ERR_2TO1(sts);	/* connect - send these as down-rev */
	sts = __pmSendError(fd, PDU_BINARY, sts);
	if (sts < 0)
	    fprintf(stderr, "error sending connection access NACK to client: %s\n",
			 pmErrStr(sts));
	__pmResetIPC(fd);
	close(fd);
	return 0;
    }

    /* encode pdu version in the acknowledgement */
    sts = __pmSendError(fd, PDU_BINARY, LOG_PDU_VERSION);
    if (sts < 0) {
	fprintf(stderr, "error sending connection ACK to client: %s\n",
		     pmErrStr(sts));
	__pmResetIPC(fd);
	close(fd);
	return 0;
    }
    clientfd = fd;
    return 1;
}
