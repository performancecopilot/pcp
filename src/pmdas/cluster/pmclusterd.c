/*
 * PCP cluster daemon. Use with the cluster PMDA on lead node.
 *
 * Copyright (c) 2008-2009 Silicon Graphics, Inc.  All Rights Reserved.
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
 */

#include <stdio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

#include "domain.h"
#include "cluster.h"

#define BACKOFF_INC 5
#define BACKOFF_MAX 120

#define INITIAL_INTERVAL 2

static char* localCtxIbStr = "PMDA_LOCAL_IB=";

static unsigned	cmdbuf[CMDBUFLEN];

static int	push_interval = INITIAL_INTERVAL;

static int
connect_leader(char *host, short port)
{
    int			fd;
    struct sockaddr_in	addr;
    struct hostent	*hostInfo;
    int			i;
    struct linger	noLinger = {1, 0};

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    	return fd;

    i = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &i,
	    (mysocklen_t)sizeof(i)) < 0) {
	fprintf(stderr, "pmclusterd: Warning: setsockopt(nodelay): %s\n", strerror(errno));
        /* ignore */
    }

    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &noLinger, (mysocklen_t)sizeof(noLinger)) < 0) {
	fprintf(stderr, "pmclusterd: Warning: setsockopt(nolinger): %s\n", strerror(errno));
        /* ignore */
    }

    hostInfo = gethostbyname(host);
    if (hostInfo == NULL) {
	fprintf(stderr, "pmclusterd: Error getting inet address for host '%s'\n", host);
	return PM_ERR_NOTHOST;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, hostInfo->h_addr, hostInfo->h_length);
    addr.sin_port = htons(port);
    return (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
	? -errno : fd;
}

int
get_config(int fd, pmID **pmidlist)
{
    int		i;
    int		npmidlist;
    pmID	*list;
    unsigned	cmdlen;

    /*
     * Read pmid list from server
     * PDU: CLUSTER_PDU_CONFIG interval pmidcount pmid [pmid ...]
     * FMT: int              int      int       int   int  ...
     */

    if (!cluster_node_read_ok(fd, cmdbuf, 3 * sizeof(cmdbuf[0])) ||
        ntohl(cmdbuf[0]) != CLUSTER_PDU_CONFIG) {
    	fprintf(stderr,"pmclusterd: unexpected PDU %u: expected CONFIG\n", cmdbuf[0]);
	exit(1);
    }
    push_interval = ntohl(cmdbuf[1]);
    npmidlist = ntohl(cmdbuf[2]);

    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "pmclusterd: push interval is %d seconds\n", (int)push_interval);
	fprintf(stderr, "pmclusterd: waiting for %d pmids\n", npmidlist);
    }

    cmdlen = npmidlist * sizeof(pmID);
    list = (pmID *)malloc(cmdlen);
    memset(list, 0, cmdlen);
    if (!cluster_node_read_ok(fd, list, cmdlen)) {
	perror("pmclusterd: Error receiving metric table config");
	exit(1);
    }
    for (i=0; i < npmidlist; i++) {
    	list[i] = ntohl(list[i]);
    }
    *pmidlist = list;

    if (pmDebug & DBG_TRACE_APPL2)
	fprintf(stderr, "pmclusterd: completed, received config for %d pmids\n", npmidlist);

    return npmidlist;
}

int
send_indoms(int fd, int npmidlist, pmID *pmidlist)
{
    int		i;
    int		j;
    int		n;
    int		sts;
    pmInDom	*indoms;
    pmDesc	desc;
    char	**namelist;
    int		*instlist;

    indoms = (pmInDom *)malloc(npmidlist * sizeof(int));
    for (i=0; i < npmidlist; i++) {
    	indoms[i] = PM_INDOM_NULL;
    }

    for (i=0; i < npmidlist; i++) {
	__pmID_int id = *__pmid_int(&pmidlist[i]);

    	if ((sts = pmLookupDesc(pmidlist[i], &desc)) < 0) {
	    fprintf(stderr, "pmclusterd: Error pmLookupDesc(%d:%d:%d) failed; %s\n",
		id.domain, id.cluster, id.item, pmErrStr(sts));
	    continue;
	}

	for (j=0; j < i; j++) {
	    if (indoms[j] == desc.indom)
		break; /* already sent this indom */
	}
	if (j < i)
	    continue;

	indoms[i] = desc.indom;

	if (desc.indom == PM_INDOM_NULL) {
	    /*
	     * The singular instance domain is translated into a special
	     * "cluster" instance domain at the server end.
	     */
	    continue;
	}

	n = pmGetInDom(indoms[i], &instlist, &namelist);
	if (n <= 0) {
	    fprintf(stderr, "pmclusterd: Warning: failed to lookup indom %d: %s\n",
		indoms[i], n ? pmErrStr(n) : "no instances found");
	    continue;
	}

	for (j=0; j < n; j++) {
	    /*
	     * 
	     * Send instance PDU
	     * PDU: CLUSTER_PDU_INSTANCE indom name
	     * FMT: int              int   char[24]
	     */
	    cmdbuf[0] = htonl(CLUSTER_PDU_INSTANCE);
	    cmdbuf[1] = htonl(indoms[i]);
	    cmdbuf[2] = htonl(instlist[j]);
	    strncpy((char *)&cmdbuf[3], namelist[j], CLUSTER_INSTANCENAME_MAXLEN);
	    if (!cluster_node_write_ok(fd, cmdbuf, 3 * sizeof(unsigned) + CLUSTER_INSTANCENAME_MAXLEN)) {
		fprintf(stderr, "pmclusterd: Error: failed to write instance to lead host', err %s\n",
		    pmErrStr(-errno));
		exit(1);
	    }
	    if (pmDebug & DBG_TRACE_APPL2) {
		__pmInDom_int *indom_int = (__pmInDom_int *)&indoms[i];
		fprintf(stderr, "pmclusterd: sent instance indom=%d.%d instance=%d name=\"%s\"\n",
		    indom_int->domain, indom_int->serial, instlist[j], namelist[j]);
	    }
	}

	free(instlist);
	free(namelist);
    }
    free(indoms);

    return 0;
}

static void
daemonize(char *lflag)
{
    int		childpid;
    char	logpath[128];
    int		fd0, fd1;

    /*
     * Daemonize
     */
    if (!lflag) {
	snprintf(logpath, sizeof(logpath), "%s/pmclusterd.log", pmGetConfig("PCP_LOG_DIR"));
	lflag = logpath;
    }
    if (freopen(logpath, "w", stderr) == NULL) {
    	perror(logpath);
	/* continue in spite of this error */
    }
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    if ((childpid = fork()) < 0)
        __pmNotifyErr(LOG_ERR, "StartDaemon: fork");
        /* but keep going */
    else if (childpid > 0) {
        /* parent, let her exit, but avoid ugly "Log finished" messages */
        fclose(stderr);
        exit(0);
    }

    /* not a process group leader, lose controlling tty */
    if (setsid() == -1)
        __pmNotifyErr(LOG_WARNING, "StartDaemon: setsid");
        /* but keep going */

    /* Close stdin/stdout and reopen */
    close(0);
    close(1);
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
}

int
main(int argc, char *argv[])
{
    int		fd = -1;
    int		err;
    int		npmidlist;
    pmID	*pmidlist;
    pmResult	*result = NULL;
    __pmPDU     *pb;
    unsigned	cmd;
    char	localhost[MAXHOSTNAMELEN];
    char	leaderhost[MAXHOSTNAMELEN];
    char	*debug;
    int		c;
    int		sts;
    int		errflag = 0;
    int		fflag = 0;
    char	*bflag = NULL;
    char	*hflag = NULL;
    char	*lflag = NULL;
    int		pflag = CLUSTER_CLIENT_PORT;
    uint32_t	rflag = 0xffffffff;
    char	*endnum;
    char	*opts = "fl:D:b:h:p:r:?";
    size_t	bflagsz;
    int		tries = 0;
    int		lasterr = 0;
    int		backoff = 0;
    fd_set      fds;

    if ((debug = getenv("pmDebug")) != NULL)
	pmDebug = atoi(debug);

    while ((c = getopt(argc, argv, opts)) != EOF) {
	switch (c) {

	    case 'p':		/* port */
		pflag = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, "pmclusterd: -p requires numeric argument\n");
		    errflag++;
		}
		break;

	    case 'r':		/* connection retries */
		rflag = (uint32_t)strtoul(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, "pmclusterd: -r requires numeric argument\n");
		    errflag++;
		}
		break;

	    case 'f':
	        fflag = 1;
		break;

	    case 'D':	/* debug flag */
		sts = __pmParseDebug(optarg);
		if (sts < 0) {
		    fprintf(stderr, "pmclusterd: unrecognized debug flag specification (%s)\n", optarg);
		    errflag++;
		}
		else
		    pmDebug |= sts;
		break;

	    case 'b':
		bflag = optarg;
		break;

	    case 'h':	/* contact PMCD on this hostname */
		hflag = optarg;
		break;

	    case '?':
		if (errflag == 0) {
		    goto usage;
		}
	}
    }

    if (errflag) {
usage:
    	fprintf(stderr,
	"Usage: pmclusterd [-b bladeid] [-h leader] [-f] [-l logfile] [-D debug] [-p portnum] [-r retries]\n"
	"-f: run in foreground (do not daemonize), default log to stderr\n"
	"-b nodeid: default is the hostname of this node\n"
	"-h leader: the hostname of the local leader\n"
	"   For ICE, default is 'rXlead', where X is the rack number from -b\n"
	"-D debug, see pmdbg(1) for details\n"
	"-p portnum: portnumber for leaser connection\n"
	"-r retries: number of retries on connection\n"
	"   default: very large!\n");
	exit(1);
    }

    if (!fflag)
	daemonize(lflag);

    if (!bflag) {
	if (gethostname(localhost, sizeof(localhost)) != 0) {
	    perror("gethostname");
	    fprintf(stderr, "pmclusterd: Error getting local host name\n");
	    return -PM_ERR_NOTHOST;
	}
	bflag = localhost;
    }
    bflagsz = strlen(bflag);

    if (!hflag) {
	int	rack, iru, blade;

	if (sscanf(bflag, "r%di%dn%d", &rack, &iru, &blade) != 3) {
	    fprintf(stderr, "pmclusterd: no lead host specified\n");
	    goto usage;
	}
	snprintf(leaderhost, sizeof(leaderhost), "r%dlead", rack);
	hflag = leaderhost;
    }

    for (;;) {
	if (fd >= 0)
	    close(fd);

	fd = -1;
	FD_ZERO(&fds);
	backoff = 0;

	for (tries = 0; tries <= rflag; tries++) {
	    if ((fd = connect_leader(hflag, CLUSTER_CLIENT_PORT)) >= 0)
		goto got_fd;

	    if (fd != lasterr) {
		fprintf(stderr, "pmclusterd: failed to connect to lead host '%s': err %s\n",
			hflag, pmErrStr(fd));
		lasterr = fd;
	    }
	    if (backoff < BACKOFF_MAX)
		backoff += BACKOFF_INC;
    
	    sleep(backoff);
	    continue;
	}
	fprintf(stderr, "pmclusterd: Error: failed to connect to "
		"lead host '%s' after %" PRIu32 " retries\n",
		hflag, rflag);
	exit(1);

    got_fd:
	FD_SET(fd, &fds);
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "Connected to leader '%s'\n", hflag);
	}

	/*
	 * Send protocol version: 
	 * PDU: CLUSTER_PDU_VERSION version
	 * FMT: int             int
	 */
	cmdbuf[0] = htonl(CLUSTER_PDU_VERSION);
	cmdbuf[1] = htonl(CLUSTER_CLIENT_VERSION);
	if (!cluster_node_write_ok(fd, cmdbuf, sizeof(unsigned) * 2)) {
	    if (-errno != lasterr) {
		fprintf(stderr, "pmclusterd: Error: failed to write version to lead host '%s', err %s\n",
			hflag, pmErrStr((lasterr = -errno)));
	    }
	    continue;
	}

	/*
	 * Send id:
	 * PDU: CLUSTER_PDU_ID hostnamelen hostname
	 * FMT: int char[]
	 */
	cmdbuf[0] = htonl(CLUSTER_PDU_ID);
	cmdbuf[1] = htonl(bflagsz);

	if (!cluster_node_write_ok(fd, cmdbuf, sizeof(unsigned) * 2)
	    || !cluster_node_write_ok(fd, bflag, bflagsz)) {
	    if (-errno != lasterr) {
		fprintf(stderr, "pmclusterd: Error: failed to write id to lead host '%s', err %s\n",
			hflag, pmErrStr((lasterr = -errno)));
	    }
	    continue;
	}

	/*
	 * Set an environment variable so pmNewContext() loads the Infiniband
	 * PMDA.  Keep going if unsuccessful.  Other PMDAs may be OK.
	 */
	if (putenv(localCtxIbStr) != 0)
	    fprintf(stderr, "pmclusterd: Warning: couldn't set environment "
		    "to enable Infiniband support in local context (%s)\n",
		    localCtxIbStr);

	if ((err = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0) {
	    fprintf(stderr, "pmclusterd: Error: failed to open a local pmcd context, err %s\n", pmErrStr(err));
	    exit(1);
	}
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "Opened local context to PMDAs\n");
	}

	/*
	 * Block and wait for the metric config. This also forces serial
	 * initialization of all blades, so we don't swamp the leader.
	 */
	npmidlist = get_config(fd, &pmidlist);

	/*
	 * Send the instance domains, then a sequence of pmResults
	 */
	send_indoms(fd, npmidlist, pmidlist);

	for (;;) {
	    fd_set         tmp_fds = fds;
	    struct timeval timeout = {0};

	    result = NULL;

	    if ((err = pmFetch(npmidlist, pmidlist, &result)) < 0)
		continue;

	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "Fetch returned %d metrics: %s\n",
			result->numpmid, pmErrStr(err));
	    }

	    /*
	     * Send result PDU :
	     * PDU: CLUSTER_PDU_RESULT result
	     * FMT: int            variable length encoded pmResult
	     */
	    cmd = htonl(CLUSTER_PDU_RESULT);
	    if (!cluster_node_write_ok(fd, &cmd, sizeof(cmd))) {
		if (-errno != lasterr) {
		    fprintf(stderr, "pmclusterd: Error: failed to write RESULT PDU to lead host '%s', err %s\n",
			    hflag, pmErrStr((lasterr = -errno)));
		}
		break;
	    }

	    err = __pmEncodeResult(PDU_OVERRIDE2, result, &pb);
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "__pmEncodeResult returned len=%d : %s\n",
			((__pmPDUHdr *)pb)->len, pmErrStr(err));
	    }
	    err = __pmXmitPDU(fd, pb);

	    if (err < 0) {
		if (lasterr != err || pmDebug & DBG_TRACE_APPL2) {
		    fprintf(stderr, "__pmXmitPDU returned %s\n", pmErrStr((lasterr = err)));
		}
		break;
	    }
	    pmFreeResult(result);

	    /* 
	     * Check for SUSPEND PDU. The timeout on this serves
	     * as the periodicity for pushing RESULTs.
	     */
	    timeout.tv_sec = push_interval;
	    if (select(fd+1, &tmp_fds, NULL, NULL, &timeout) <= 0)
		continue;

	    else  if (!FD_ISSET(fd, &tmp_fds))
		fprintf(stderr, "pmclusterd: select on unrecognised fd: looking for %d\n", fd);

	    else if (!cluster_node_read_ok(fd, cmdbuf, sizeof(cmdbuf[0])))
		fprintf(stderr, "pmclusterd: error receiving SUSPEND PDU: %d\n", errno);

	    else if (htonl(cmdbuf[0]) != CLUSTER_PDU_SUSPEND)
		fprintf(stderr, "pmclusterd: unexpected PDU %u while checking for SUSPEND\n", *cmdbuf);

	    /* monitoring suspended: blocking read for RESUME */
	    else if (!cluster_node_read_ok(fd, cmdbuf, sizeof(cmdbuf[0])))
		fprintf(stderr, "pmclusterd: error receiving RESUME PDU: %d\n", errno);

	    else if (htonl(cmdbuf[0]) != CLUSTER_PDU_RESUME)
		fprintf(stderr, "pmclusterd: unexpected PDU %u: expected RESUME\n", *cmdbuf);

	    else
		continue;

	    break;
	}
    }
    exit(0);
}

