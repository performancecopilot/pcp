/*
 * Cluster PMDA main loop
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
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <syslog.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

#include "domain.h"
#include "cluster.h"

#define CLUSTER_CONTROLFILE	"/cluster/monitoring_suspended"

#define CLUSTER_CLIENT_SUSPEND_VERSION	101 /* need >= this to get PDU_SUSPEND */

#define MYBUFSZ (1<<14) /* 16k. Must be > MAXPATHLEN */

int              n_cluster_clients = 0;
cluster_client_t   *cluster_clients = NULL;

static int		maxFd = -1;
static fd_set		allFds;

static int		interval = 2; /* client push interval (seconds) */

static int		monitoring_suspended = 0;

static char		buf[MYBUFSZ];

int
cluster_request_socket(short port)
{
    int			fd;
    int			i, sts;
    struct sockaddr_in	myAddr;
    struct linger	noLinger = {1, 0};
    int			one = 1;
    __uint32_t		ipAddr = htonl(INADDR_ANY);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	__pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, 0x%x) socket: %s\n",
	    port, ipAddr, strerror(errno));
	return -1;
    }
    if (fd > maxFd)
	maxFd = fd;
    FD_SET(fd, &allFds);
    i = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &i,
		   (mysocklen_t)sizeof(i)) < 0) {
	__pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, 0x%x) setsockopt(nodelay): %s\n",
	    port, ipAddr, strerror(errno));
	close(fd);
	return -1;
    }

    /* Don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *)&noLinger, (mysocklen_t)sizeof(noLinger)) < 0) {
	__pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, 0x%x) setsockopt(nolinger): %s\n",
	    port, ipAddr, strerror(errno));
    }

    /* Ignore dead client connections */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&one, (mysocklen_t)sizeof(one)) < 0) {
	__pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, 0x%x) setsockopt(SO_REUSEADDR): %s\n",
	    port, ipAddr, strerror(errno));
    }

    /* and keep alive please - pv 916354 bad networks eat fds */
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&one, (mysocklen_t)sizeof(one)) < 0) {
	__pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, 0x%x) setsockopt(SO_KEEPALIVE): %s\n",
	    port, ipAddr, strerror(errno));
    }

    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = ipAddr;
    myAddr.sin_port = htons(port);
    sts = bind(fd, (struct sockaddr*)&myAddr, sizeof(myAddr));
    if (sts < 0){
	__pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, 0x%x) bind: %s\n", port, ipAddr, strerror(errno));
	__pmNotifyErr(LOG_ERR, "cluster daemon is already running?\n");
	close(fd);
	return -1;
    }

    sts = listen(fd, 64);	/* Max. of 64 pending connection requests */
    if (sts == -1) {
	__pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, 0x%x) listen: %s\n",
	    port, ipAddr, strerror(errno));
	close(fd);
	return -1;
    }

    /* success */
    return fd;
}

cluster_client_t *
cluster_accept_client(int reqfd)
{
    int			i, fd;
    mysocklen_t		addrlen;
    cluster_client_t	*tc;

    for (i=0; i < n_cluster_clients; i++) {
    	if (cluster_clients[i].fd < 0)
	    break; /* reclaim */
    }
    if (i == n_cluster_clients) {
        n_cluster_clients++;
	cluster_clients = (cluster_client_t *)realloc(cluster_clients,
		n_cluster_clients * sizeof(cluster_client_t));
    }
    tc = &cluster_clients[i];
    memset(tc, 0, sizeof(cluster_client_t));

    addrlen = sizeof(tc->addr);
    fd = accept(reqfd, (struct sockaddr *)&tc->addr, &addrlen);
    if (fd == -1) {
        if (errno == EPERM) {
            __pmNotifyErr(LOG_NOTICE, "AcceptNewClient(%d): "
                          "Permission Denied\n", reqfd);
            return NULL;
        }
        else {
            __pmNotifyErr(LOG_ERR, "AcceptNewClient(%d) accept: %s\n",
            reqfd, strerror(errno));
            exit(1);
        }
    }
    if (fd > maxFd)
        maxFd = fd;

    FD_SET(fd, &allFds);
    tc->fd = fd;
    __pmSetVersionIPC(fd, LOG_PDU_VERSION2);

    return tc;
}


static void
cluster_kill_client(cluster_client_t *tc, int err)
{
    cluster_inst_t *inst;

    if (tc == NULL)
	return;

    if (err && (pmDebug & DBG_TRACE_APPL2)) {
	__pmNotifyErr(LOG_NOTICE, "Client %d: disconnected: %s\n", tc->fd, pmErrStr(err));
    }
    for (inst = tc->instlist; inst; inst = inst->next)
	inst->client = -1;

    if (tc->result)
    	pmFreeResult(tc->result);

    if (tc->fd > 0) {
	FD_CLR(tc->fd, &allFds);
	close(tc->fd);
    }
    if (tc->metrics) {
	free(tc->metrics);
    }
    if (tc->host) {
	free(tc->host);
    }
    memset(tc, 0, sizeof(cluster_client_t));
    tc->fd = -1;
}

/* inst should be the special node instance,
 * which is always at the head of the list.
 */
static void
cluster_delete_instances(cluster_inst_t *inst)
{
    cluster_inst_t *nextinst;
    char *instname;
    int sts;

    while (inst) {
	instname = NULL;
	sts = pmdaCacheLookup(inst->indom, inst->inst, &instname, NULL);
	if (sts == PMDA_CACHE_ACTIVE)
	    pmdaCacheStore(inst->indom, PMDA_CACHE_CULL, instname, NULL);

	nextinst = inst->next;
	free(inst);
	inst = nextinst;
    }
}

#define CLIENT_READ_TEST(cond, str, err) do { \
    if ((cond)) { \
	int sav = err; \
	perror(str); \
	cluster_kill_client(tc, sav); \
	return; \
    } \
} while (0)

static void
indom_save(pmInDom indom)
{
    static pmInDom indom_to_save = PM_INDOM_NULL;

    if (indom_to_save != indom) {
	if (indom_to_save != PM_INDOM_NULL) {
	    pmdaCacheOp(indom_to_save, PMDA_CACHE_SAVE);
	}
	indom_to_save = indom;
    }
}

char *
cluster_client_metrics_get(char *host)
{
    /*
     * Read the node config file
     */
    FILE	*fp;
    ssize_t	totl = 0;

    snprintf(buf, sizeof(buf), "%s/cluster/nodes/%s", pmGetConfig("PCP_PMDAS_DIR"), host);
    if ((fp = fopen(buf, "r")) == NULL) {
	/* Fall back to default config */
	snprintf(buf, sizeof(buf), "%s/cluster/config", pmGetConfig("PCP_PMDAS_DIR"));
	if ((fp = fopen(buf, "r")) == NULL) {
	    perror(buf);
	    exit(1); /* FATAL */
	}
    }
    while (totl < MYBUFSZ - 1 && NULL != 
	   fgets(&buf[totl], MYBUFSZ - totl, fp)) 
    {
	if (buf[totl] != '#') 
	    totl += strlen(&buf[totl]);
    }
    fclose(fp);

    return buf;
}

static char *
cluster_client_metrics_cache(cluster_client_t *tc)
{
    if (tc->host == NULL)
	return NULL;

    if (tc->metrics == NULL)
	tc->metrics = strdup(cluster_client_metrics_get(tc->host));

    return tc->metrics;
}

int
cluster_client_metrics_set(pmValue *valp, int delete)
{
    int			sts = 0;
    cluster_inst_t	*instp;
    char		*instname;
    cluster_client_t	*tc = NULL;
    FILE		*fp = NULL;
    static int		indom = 0;
    static __pmInDom_int	*indom_int;
    int i;
    
    if (indom == 0) {
	indom_int = (__pmInDom_int *)&(indom);
	indom_int->serial = CLUSTER_INDOM;
	indom_int->domain = CLUSTER;
    }
    /*
     * find the name and client for this instance==host in the pmda cache
     */
    sts = pmdaCacheLookup(indom, valp->inst, &instname, (void **)&instp);
    if (sts != PMDA_CACHE_ACTIVE) {
	fprintf(stderr, "Error: pmdaCacheLookup Error: %s, cluster.control.metrics inst=0x%x\n",
		pmErrStr(sts), valp->inst);
	return PM_ERR_INST;
    }
    if (instp->client >= 0)
	tc = &cluster_clients[instp->client];

    if (delete) {
	if (1 != (int)valp->value.lval) {
	    fprintf(stderr, "cluster_client_metrics_set: Error: attempt to set "
		    "cluster.control.delete to %d\n", valp->value.lval);
	    return PM_ERR_VALUE;
	}
	if (tc && tc->host && strcmp(tc->host, instname) == 0) {
	    fprintf(stderr, "cluster_client_metrics_set: Error: "
		    "cannot delete instances for a running node.\n");
	    return PM_ERR_ISCONN;
	}
	snprintf(buf, sizeof(buf), "%s/cluster/nodes/%s", pmGetConfig("PCP_PMDAS_DIR"), instname);
	unlink(buf);
	cluster_delete_instances(instp);
	return 0;
    }
    for (i=0; i < ncluster_mtab; i++) {
	if (strstr(valp->value.pval->vbuf, (char *)cluster_mtab[i].m_user) == NULL)
	    continue;

	/* Theres at least one valid metric: allow the change */
	sprintf(buf, "%s/cluster/nodes/%s", pmGetConfig("PCP_PMDAS_DIR"), instname);
	if (NULL != (fp = fopen(buf, "w"))) {
	    fputs(valp->value.pval->vbuf,fp);
	    fclose(fp);
	}
	cluster_kill_client(tc, 0);
	return 0;
    }
    return PM_ERR_NAME;
}

void
cluster_client_read(cluster_client_t *tc, int ic)
{
    int			sts;
    __pmPDU		*pb;
    unsigned		cmd;
    unsigned		cmdlen;
    unsigned		cmdbuf[CMDBUFLEN];
    pmInDom		indom = 0;
    __pmInDom_int	*indom_int;
    int			inst, node_inst;
    cluster_inst_t	*instp = NULL;
    char		*instname;
    char		newname[CLUSTER_INSTANCENAME_MAXLEN];
    int			tries = 0;
    __pmInDom_int_subdomain
			*indom_int_sub;
    unsigned int	subdom;

    if (!cluster_node_read_ok(tc->fd, &cmd, sizeof(cmd))) {
	fprintf(stderr, "Error: cluster_client_read: fd=%d failed to read cmd: err=%d\n", tc->fd, errno);
	cluster_kill_client(tc, -errno);
    	return;
    }

    switch (ntohl(cmd)) {
    case CLUSTER_PDU_VERSION:
        /*
	 * client sends version
	 */
	CLIENT_READ_TEST(!cluster_node_read_ok(tc->fd, cmdbuf, sizeof(cmdbuf[0])),
			 "bad CLUSTER_PDU_VERSION read from client", -errno);
	tc->version = ntohl(cmdbuf[0]);
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "Client %d: protocol version %d\n", tc->fd, tc->version);
	}
	break;

    case CLUSTER_PDU_ID:
	/*
	 * client sends hostnamelen hostname[]
	 * and then blocks waiting for us to send CLUSTER_PDU_CONFIG
	 */
	CLIENT_READ_TEST(!cluster_node_read_ok(tc->fd, &cmdlen, sizeof(cmdlen)),
			 "bad CLUSTER_PDU_ID read from client: failed to read hostname length", -errno);

	cmdlen = ntohl(cmdlen);
	CLIENT_READ_TEST(!cmdlen || cmdlen > MAXHOSTNAMELEN,
			 "bad CLUSTER_PDU_ID read from client: bad hostname length", -EOVERFLOW);

	tc->host = malloc(cmdlen+1);
	CLIENT_READ_TEST(!tc->host, "malloc failed", -errno);
	CLIENT_READ_TEST(!cluster_node_read_ok(tc->fd, tc->host, cmdlen),
			 "bad CLUSTER_PDU_ID read from client", -errno);
	tc->host[cmdlen] = '\0';
	/*
	 * Add the special cluster_node instance to the CLUSTER_INDOM indom.
	 * This is for metrics with singular instance domain (they are
	 * no longer singular once aggregated by this PMDA).
	 */
	indom_int = __pmindom_int(&indom);
	indom_int->serial = CLUSTER_INDOM;
	indom_int->domain = CLUSTER;
	/*
	 * Sometimes we seem not to notice a client going away, so if we've
	 * received a PDU_ID where we thought there was already a client,
	 * first try to clean up the old one. But if it won't go, instead
	 * abandon the attempt to start a new one 
	 */
	while ((sts = pmdaCacheLookupName(indom, tc->host, &inst, (void **)&instp))
	       == PMDA_CACHE_ACTIVE && instp && instp->client >= 0 
	       && instp->client < n_cluster_clients 
	       && cluster_clients[instp->client].fd != -1) 
	{
	    if (!tries++) {
		if (pmDebug & DBG_TRACE_APPL2) {
		    fprintf(stderr, "Warning: Client %s already connected: kill old connection\n", tc->host);
		}
		cluster_kill_client(&cluster_clients[instp->client], -EADDRINUSE);
		instp = NULL;
		continue;
	    }
	    fprintf(stderr, "Error: Client %s already connected: cant kill\n", tc->host);
	    cluster_kill_client(tc, -EADDRINUSE);
	    return;
	}
	if (sts < 0 && sts != PM_ERR_INST) {
	    fprintf(stderr, "Error: Client %s lookup failed\n", tc->host);
	    cluster_kill_client(tc, sts);
	    return;
	}
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "Client %d: id %s\n", tc->fd, tc->host);
	}
	if (instp == NULL) {
	    instp = (cluster_inst_t *)malloc(sizeof(cluster_inst_t));
	    CLIENT_READ_TEST(!instp, "malloc failed", -errno);
	    sts = 0;
	    instp->next = NULL;
	    instp->indom = indom;
	    instp->node_inst = PM_INDOM_NULL;
	}
	instp->client = ic;
	tc->instlist = instp;

	if (sts != PMDA_CACHE_ACTIVE) {
	    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, tc->host, (void *)instp);
	    if (inst < 0) {
		fprintf(stderr, "Client %d: pmdaCacheStore failed: indom=CLUSTER:CLUSTER err=%s\n",
			tc->fd, pmErrStr(inst));
	    }
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "... added cluster_node instance %s, inst=%d\n", tc->host, sts);
	    }
	}
	instp->inst = inst;
	/*
	 * Flag that this client needs a pmid list sent
	 */
	tc->flags |= CLUSTER_CLIENT_FLAG_WANT_CONFIG;
	tc->metrics = NULL;

	break;

    case CLUSTER_PDU_INSTANCE:
	/*
	 * Client sends indom, instance, name[]
	 */
	CLIENT_READ_TEST(!cluster_node_read_ok(tc->fd, cmdbuf,
				sizeof(cmdbuf[0]) + sizeof(cmdbuf[1]) + CLUSTER_INSTANCENAME_MAXLEN), \
			 "bad CLUSTER_PDU_INSTANCE read from client", -errno);
	indom = ntohl(cmdbuf[0]);
	node_inst = ntohl(cmdbuf[1]);
	instname = (char *)&cmdbuf[2];
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr,
		    "Client %d: instance PDU indom=%d inst=%d name=\"%s\"\n",
		    tc->fd, indom, node_inst, instname);
	}

	/*
	 * Insert the subdomain into instance.serial's high bits, checking
	 * that the client isn't breaking protocol by using the bits itself.
	 */
	indom_int = __pmindom_int(&indom);
	indom_int_sub = __pmindom_int_subdomain(&indom);
	if (indom == PM_INDOM_NULL
	    || (subdom = indom_int_sub->subdomain) != 0
	    || indom_int->domain >= num_dom_subdom_map
	    || ((subdom = dom_subdom_map[indom_int->domain])
		>= num_subdom_dom_map)) 
	{
	    const char* subname =
		((subdom < num_subdom_name_map) ?
		 subdom_name_map[subdom] : "?");
	    fprintf(stderr, "cluster PMDA: warning: dropping indom "
		    "%d.%d from sub-PMDA %s as client domain cannot "
		    "be mapped to a subdomain\n",
		    indom_int->domain, indom_int->serial,  subname);
	    break;
	}
	/* translate the instance domain */
	indom_int_sub->subdomain = subdom;
	indom_int->domain = CLUSTER;

	/* translate external instance name */
	snprintf(newname, sizeof(newname), "%s-%d %s", tc->host, node_inst,
		 instname);

	/* load each indom once */
	if (!pmdaCacheOp(indom, PMDA_CACHE_CHECK)) {
	    pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	}
	sts = pmdaCacheLookupName(indom, newname, &inst, (void **)&instp);
	if (sts < 0 && sts != PM_ERR_INST) {
	    fprintf(stderr,
		    "Error: Client %s instance %s lookup failed\n",
		    tc->host, newname);
	    return;
	}
	else if (sts == PMDA_CACHE_ACTIVE && instp) {
	    if (inst != instp->inst)
		fprintf(stderr, "Error: Client %s instance mismatch: %d to %d?\n", 
			tc->host, instp->inst, inst);

	    instp->client = ic;
	    break;
	}
	instp = (cluster_inst_t *)malloc(sizeof(cluster_inst_t));
	CLIENT_READ_TEST(!instp, "malloc failed", -errno);
	sts = 0;

	/* Add the instance into the list (used when client disconnects).
	 * The special node instance should already be present and first.
	 */
	instp->next	   = tc->instlist->next;
	tc->instlist->next = instp;
	instp->indom	   = indom;
	instp->node_inst   = node_inst;
	instp->client	   = ic;
	indom_save(indom);

	sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, newname, (void *)instp);
	if (sts < 0) {
	    fprintf(stderr, "Client %d: pmdaCacheStore failed: indom=%d:%d inst=%d name=\"%s\" err=%s\n",
		    tc->fd, indom_int->domain, indom_int->serial, node_inst, newname, pmErrStr(sts));
	}
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "Client %d: added cached instance indom=%d:%d node_inst=%d inst=%d name=\"%s\"\n",
		    tc->fd, indom_int->domain, indom_int->serial, node_inst, sts, newname);
	}
	instp->inst = sts;
	break;

    case CLUSTER_PDU_RESULT:
	/*
	 * client sends an encoded pmResult
	 */
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "Client %d: CLUSTER_PDU_RESULT ...\n", tc->fd);
	}

	indom_save(PM_INDOM_NULL); /* 1st RESULT saves cache of last indom */
	pb = tc->pb;
	sts = __pmGetPDU(tc->fd, PDU_BINARY, 60, &tc->pb);
	__pmPinPDUBuf(tc->pb);
	if (pb)
	    __pmUnpinPDUBuf(pb);

	if (sts != PDU_RESULT) {
	    cluster_kill_client(tc, PM_ERR_IPC);
	    return;
	}
	/* Clients look for SUSPEND or CONFIG after sending a result */
	if (monitoring_suspended && tc->version >= CLUSTER_CLIENT_SUSPEND_VERSION)
	{
	    cmdbuf[0] = htonl(CLUSTER_PDU_SUSPEND);
	    if (tc->pb) {
		__pmUnpinPDUBuf(tc->pb);
		tc->pb = NULL;
	    }
	    if (!cluster_node_write_ok(tc->fd, cmdbuf, sizeof(*cmdbuf))) {
		fprintf(stderr, "Error writing PDU_SUSPEND to client %d\n", tc->fd);
	    } else {
		tc->flags |= CLUSTER_CLIENT_FLAG_SUSPENDED;

		if (pmDebug & DBG_TRACE_APPL2) {
		    fprintf(stderr, "Suspending client %d\n", tc->fd);
		}
	    }
	}
	else if (tc->result) {
	    /*
	     * This result is now stale.
	     * The new pdu is decoded in pmda.c
	     * only if needed (i.e. fetched).
	     */
	    tc->flags |= CLUSTER_CLIENT_FLAG_STALE_RESULT;
	}
	break;
    }
}

static int
cluster_send_config(cluster_client_t *tc)
{
    static unsigned 	*sendbuf = NULL;
    int i, j;

    /*
     * Write pmid list to client.
     * PDU: CLUSTER_PDU_CONFIG seconds pmidcount pmid [pmid ...]
     * FMT: int        int     int       int   int  ...
     */
    if (sendbuf == NULL) {
	/*
	 * allocate buffer bug enough for all (non-control) metrics,
	 * plus header
	 */
	sendbuf = (unsigned *)malloc((ncluster_mtab + 3) * sizeof(sendbuf[0]));
	if (!sendbuf) {
	    perror("malloc failed");
	    return -errno;
	}
	sendbuf[0] = htonl(CLUSTER_PDU_CONFIG);
	sendbuf[1] = htonl(interval);
    }
    for (i=0, j=3; i < ncluster_mtab; i++) {
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "... sending config [%d] \"%s\" to client %d\n",
		    i, (char *)cluster_mtab[i].m_user, tc->fd);
	}
	if (strstr(cluster_client_metrics_cache(tc),
		   (char *)cluster_mtab[i].m_user) != NULL) {

	    sendbuf[j++] = htonl(subcluster_mtab[i]);
	}
    }
    sendbuf[2] = htonl(j-3); /* number of configured metrics */

    if (!cluster_node_write_ok(tc->fd, sendbuf, j * sizeof(sendbuf[0]))) {
        perror("Error writing metric table to client");
        return -errno;
    }

    if (pmDebug & DBG_TRACE_APPL2) {
    	fprintf(stderr, "Wrote config to client %d\n", tc->fd);
    }
    /* success */
    tc->flags &= ~CLUSTER_CLIENT_FLAG_WANT_CONFIG;
    return 0;
}

int
cluster_monitoring_suspended(void)
{
    return monitoring_suspended;
}

void
cluster_suspend_monitoring(int req)
{
    FILE		*control_file;
    cluster_client_t	*tc;
    int			i;
    unsigned		cmd;

    if (req == monitoring_suspended)
	return;

    monitoring_suspended = req;

    /* Clients look for suspension after they have sent a result,
     * so send suspends after *receiving* results. This reduces
     * potential complications in the protocol
     */
    for (i=0; i < n_cluster_clients; i++) {
	tc = &cluster_clients[i];
	if (tc->fd < 0 || tc->version < CLUSTER_CLIENT_SUSPEND_VERSION) 
	    continue;

	if (req) { /* cause fetches to return PM_ERR_VALUE (stale) */
	    if (tc->pb) {
		__pmUnpinPDUBuf(tc->pb);
		tc->pb = NULL;
	    }
	} else if (cluster_clients[i].flags & CLUSTER_CLIENT_FLAG_SUSPENDED) {
	    cmd = htonl(CLUSTER_PDU_RESUME);

	    if (!cluster_node_write_ok(tc->fd, &cmd, sizeof(cmd))) {
		fprintf(stderr, "Error writing PDU_RESUME to client %d\n", tc->fd);
	    } else if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "Suspending client %d\n", tc->fd);
	    }
	    cluster_clients[i].flags &= ~CLUSTER_CLIENT_FLAG_SUSPENDED;
	}
    }
    sprintf(buf, "%s" CLUSTER_CONTROLFILE, pmGetConfig("PCP_PMDAS_DIR"));
    if (NULL != (control_file = fopen(buf, "w"))) {
	fprintf(control_file, "%d\n", monitoring_suspended);
	fclose(control_file);
    }
    return;
}

void
cluster_main_loop(pmdaInterface *dispatch)
{
    int			i;
    int			ready_count;
    int			sts;
    int			reqFd;
    int			inFd;
    fd_set		readableFds;
    cluster_client_t	*tc;
    FILE		*control_file;

    if((inFd = __pmdaInFd(dispatch)) < 0) {
	__pmNotifyErr(LOG_ERR,
		      "Could not obtain infd: %s\n",
		      pmErrStr(inFd));
	exit(1);
    }
    maxFd = inFd;
    FD_ZERO(&allFds);
    FD_SET(inFd, &allFds);
    reqFd = cluster_request_socket(CLUSTER_CLIENT_PORT);

    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "cluster_main_loop started, listening on port %d\n", CLUSTER_CLIENT_PORT);
    }

    sprintf(buf, "%s" CLUSTER_CONTROLFILE, pmGetConfig("PCP_PMDAS_DIR"));
    if (NULL != (control_file = fopen(buf, "r"))) {
	fscanf(control_file, "%d", &monitoring_suspended);
	fclose(control_file);
    }
    for (;;) {
	readableFds = allFds;

	if ((sts = select(maxFd+1, &readableFds, NULL, NULL, NULL)) > 0) {

	    if (FD_ISSET(inFd, &readableFds) &&
		(sts = __pmdaMainPDU(dispatch)) < 0) {
		__pmNotifyErr(LOG_ERR,
			      "pmdaMainPDU failed: %s\n",
			      pmErrStr(sts));
		break;
	    }

	    /*
	     * New clients connect on reqFd. We process these ahead of
	     * existing clients.
	     */
	    if (FD_ISSET(reqFd, &readableFds)) {
		tc = cluster_accept_client(reqFd);
		if (pmDebug & DBG_TRACE_APPL2) {
		    __pmNotifyErr(LOG_NOTICE, "New Client: => %d\n", tc->fd);
		}
	    }

	    for (ready_count=0, i=0; i < n_cluster_clients; i++) {
		tc = &cluster_clients[i];
		if (tc->fd < 0 || (tc->flags & CLUSTER_CLIENT_FLAG_WANT_CONFIG)) 
		    continue;

		if (FD_ISSET(tc->fd, &readableFds)) {
		    /* TODO: This should be made non-blocking so that failure
		     * to get a response from a single client doesnt cause the
		     * pmda to block and get killed
		     */
		    cluster_client_read(tc, i);
		    ready_count++;
		}
	    }

	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "Main loop: %d clients were ready\n", ready_count);
	    }
	    for (i=0; i < n_cluster_clients; i++) {
		tc = &cluster_clients[i];
		if (tc->fd >= 0 && (tc->flags & CLUSTER_CLIENT_FLAG_WANT_CONFIG)) {
		    /*
		     * Client is blocked waiting for it's config
		     */
		    if (pmDebug & DBG_TRACE_APPL2) {
			fprintf(stderr, "Sending config to client %d\n", tc->fd);
		    }
		    cluster_send_config(tc);
		}
	    }
	}
    }
}
