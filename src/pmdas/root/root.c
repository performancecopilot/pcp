/*
 * PMCD privileged co-process (root) PMDA.
 *
 * Copyright (c) 2014-2015 Red Hat.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <sys/stat.h>
#include "root.h"
#include "lxc.h"
#include "docker.h"
#include "domain.h"

#ifndef S_IRWXU
/*
 * for Linux we have this ...
 * #define S_IRWXU	(__S_IREAD|__S_IWRITE|__S_IEXEC)
 * fake up something equivalent for others
 */
#define S_IRWXU 0700
#endif

static char socket_path[MAXPATHLEN];
static __pmSockAddr *socket_addr;
static int socket_fd = -1;
static int pmcd_fd = -1;

static __pmFdSet connected_fds;
static int maximum_fd;

static const int features = PDUROOT_FLAG_HOSTNAME | \
			    PDUROOT_FLAG_PROCESSID | \
			    PDUROOT_FLAG_CGROUPNAME;

static pmdaIndom root_indomtab[NUM_INDOMS];
#define INDOM(x) (root_indomtab[x].it_indom)
#define INDOMTAB_SZ (sizeof(root_indomtab)/sizeof(root_indomtab[0]))

static pmdaMetric root_metrictab[] = {
    { NULL, { PMDA_PMID(0, CONTAINERS_ENGINE), PM_TYPE_STRING,
	CONTAINERS_INDOM, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL, { PMDA_PMID(0, CONTAINERS_NAME), PM_TYPE_STRING,
	CONTAINERS_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL, { PMDA_PMID(0, CONTAINERS_PID), PM_TYPE_U32,
	CONTAINERS_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL, { PMDA_PMID(0, CONTAINERS_RUNNING), PM_TYPE_U32,
	CONTAINERS_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL, { PMDA_PMID(0, CONTAINERS_PAUSED), PM_TYPE_U32,
	CONTAINERS_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL, { PMDA_PMID(0, CONTAINERS_RESTARTING), PM_TYPE_U32,
	CONTAINERS_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
};
#define METRICTAB_SZ (sizeof(root_metrictab)/sizeof(root_metrictab[0]))

static container_engine_t engines[] = {
#ifdef IS_LINUX
    {
	.name		= "docker",
	.setup		= docker_setup,
	.indom_changed	= docker_indom_changed,
	.insts_refresh	= docker_insts_refresh,
	.value_refresh	= docker_value_refresh,
	.name_matching	= docker_name_matching,
    },
    {
	.name		= "lxc",
	.setup		= lxc_setup,
	.indom_changed	= lxc_indom_changed,
	.insts_refresh	= lxc_insts_refresh,
	.value_refresh	= lxc_value_refresh,
	.name_matching	= lxc_name_matching,
    },
#endif
    { .name = NULL },
};

static void
root_setup_containers(void)
{
    container_engine_t *dp;

    for (dp = &engines[0]; dp->name != NULL; dp++)
	dp->setup(dp);
}

static void
root_refresh_container_indom(void)
{
    int need_refresh = 0;
    container_engine_t *dp;
    pmInDom indom = INDOM(CONTAINERS_INDOM);

    for (dp = &engines[0]; dp->name != NULL; dp++)
	need_refresh |= dp->indom_changed(dp);
    if (!need_refresh)
	return;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
    for (dp = &engines[0]; dp->name != NULL; dp++)
	dp->insts_refresh(dp, indom);
}

static int
root_refresh_container_values(char *container, container_t *values)
{
    container_engine_t *dp;

    for (dp = &engines[0]; dp->name != NULL; dp++) {
	if (values->engine != dp)
	    continue;
	return dp->value_refresh(dp, container, values);
    }
    return PM_ERR_INST;
}

container_t *
root_container_search(const char *query)
{
    int inst, fuzzy, best = 0;
    char *name = NULL;
    container_t *cp = NULL, *found = NULL;
    container_engine_t *dp;
    pmInDom indom = INDOM(CONTAINERS_INDOM);

    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((inst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(indom, inst, &name, (void **)&cp) || !cp)
	    continue;
	if (root_refresh_container_values(name, cp) < 0 || !query)
	    continue;
	for (dp = &engines[0]; dp->name != NULL; dp++) {
	    if ((fuzzy = dp->name_matching(dp, query, cp->name, name)) <= best)
		continue;
	    if (pmDebug & DBG_TRACE_ATTR)
		__pmNotifyErr(LOG_DEBUG, "container search: %s/%s (%d->%d)\n",
				query, name, best, fuzzy);
	    best = fuzzy;
	    found = cp;
	}
    }

    if (pmDebug & DBG_TRACE_ATTR) {
	if (found) /* query must be non-NULL */
	    __pmNotifyErr(LOG_DEBUG, "found container: %s (%s/%d) pid=%d\n",
				name, query, best, found->pid);
	else /* query may be NULL */
	    __pmNotifyErr(LOG_DEBUG, "container %s not matched\n", query ? query : "NULL");
    }

    return found;
}

static int
root_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    __pmInDom_int	*indomp = (__pmInDom_int *)&indom;

    if (indomp->serial == CONTAINERS_INDOM)
	root_refresh_container_indom();
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
root_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    root_refresh_container_indom();
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
root_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    container_t		*cp;
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    pmInDom		containers;
    char		*name;
    int			sts;

    switch (idp->cluster) {
    case 0:	/* container metrics */
	containers = INDOM(CONTAINERS_INDOM);
	sts = pmdaCacheLookup(containers, inst, &name, (void**)&cp);
	if (sts < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	    return PM_ERR_INST;
	if (root_refresh_container_values(name, cp) < 0)
	    return PM_ERR_INST;
	switch (idp->item) {
	case 0:		/* containers.engine */
	    atom->cp = cp->engine->name;
	    break;
	case 1:		/* containers.name */
	    atom->cp = *cp->name == '/' ? cp->name+1 : cp->name;
	    break;
	case 2:		/* containers.pid */
	    atom->ul = cp->pid;
	    break;
	case 3:		/* containers.state.running */
	    atom->ul = (cp->flags & CONTAINER_FLAG_RUNNING) != 0;
	    break;
	case 4:		/* containers.state.paused */
	    atom->ul = (cp->flags & CONTAINER_FLAG_PAUSED) != 0;
	    break;
	case 5:		/* containers.state.restarting */
	    atom->ul = (cp->flags & CONTAINER_FLAG_RESTARTING) != 0;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    default:
	return PM_ERR_PMID;
    }

    return 1;
}

/* General utility routine for checking timestamp differences */
int
root_stat_time_differs(struct stat *statbuf, struct stat *lastsbuf)
{
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
    if (statbuf->st_mtime != lastsbuf->st_mtime)
	return 1;
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
    if ((statbuf->st_mtimespec.tv_sec != lastsbuf->st_mtimespec.tv_sec) ||
	(statbuf->st_mtimespec.tv_nsec != lastsbuf->st_mtimespec.tv_nsec))
	return 1;
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
    if ((statbuf->st_mtim.tv_sec != lastsbuf->st_mtim.tv_sec) ||
	(statbuf->st_mtim.tv_nsec != lastsbuf->st_mtim.tv_nsec))
	return 1;
#else
!bozo!
#endif
    return 0;
}

#if defined(HAVE_STRUCT_SOCKADDR_UN)
static void
root_setup_socket(void)
{
    int			fd, sts;
    char		path[MAXPATHLEN];
    const int		backlog = 5;

    /*
     * Create a Unix domain socket, if supported, for privilege isolation.
     * We use this PMDA as a server to other PMDAs needing namespace file
     * descriptors (namespace files opened by root) for extracing metrics
     * from within a container.  The central mechanism used is SCM_RIGHTS
     * from unix(7).
     */
    if ((fd = __pmCreateUnixSocket()) < 0) {
	__pmNotifyErr(LOG_ERR, "setup: __pmCreateUnixSocket failed: %s\n",
			netstrerror());
	exit(1);
    }
    if ((socket_addr = __pmSockAddrAlloc()) == NULL) {
	__pmNotifyErr(LOG_ERR, "setup: __pmSockAddrAlloc out of memory\n");
	__pmCloseSocket(fd);
	exit(1);
    }

    if (socket_path[0] == '\0')
	snprintf(socket_path, sizeof(socket_path), "%s/pmcd/root.socket",
		pmGetConfig("PCP_TMP_DIR"));
    unlink(socket_path);
    memcpy(path, socket_path, sizeof(path));	/* dirname copy */
    sts = __pmMakePath(dirname(path), S_IRWXU);
    if (sts < 0 && oserror() != EEXIST) {
	__pmNotifyErr(LOG_ERR, "setup: __pmMakePath failed on %s: %s\n",
			dirname(socket_path), osstrerror());
	__pmSockAddrFree(socket_addr);
	__pmCloseSocket(fd);
	exit(1);
    }

    __pmSockAddrSetFamily(socket_addr, AF_UNIX);
    __pmSockAddrSetPath(socket_addr, socket_path);
    __pmServerSetLocalSocket(socket_path);

    sts = __pmBind(fd, (void *)socket_addr, __pmSockAddrSize());
    if (sts < 0) {
	__pmNotifyErr(LOG_ERR, "setup: __pmBind failed: %s\n", netstrerror());
	__pmCloseSocket(fd);
	unlink(socket_path);
	exit(1);
    }

    sts = __pmListen(fd, backlog);
    if (sts < 0) {
	__pmNotifyErr(LOG_ERR, "setup: __pmListen failed: %s\n", netstrerror());
	__pmCloseSocket(fd);
	unlink(socket_path);
	exit(1);
    }

    if (fd > maximum_fd)
	maximum_fd = fd;

    __pmFD_ZERO(&connected_fds);
    __pmFD_SET(fd, &connected_fds);
    socket_fd = fd;
}

static void
root_close_socket(void)
{
    char	errmsg[PM_MAXERRMSGLEN];

    if (socket_fd >= 0) {
	__pmCloseSocket(socket_fd);
	if (unlink(socket_path) != 0 && oserror() != ENOENT) {
	    __pmNotifyErr(LOG_ERR, "%s: cannot unlink %s: %s",
			  pmProgname, socket_path,
			  osstrerror_r(errmsg, sizeof(errmsg)));
	}
	socket_fd = -EPROTO;
    }
}
#else
static inline void root_setup_socket(void) { }
static inline void root_close_socket(void) { }
#endif


typedef struct {
    int		fd;
} root_client_t;

static root_client_t *client;
static int client_size;	/* highwater allocation mark */
static int nclients;
static const int MIN_CLIENTS_ALLOC = 8;

static int
root_new_client(void)
{
    int i, sz;

    for (i = 0; i < nclients; i++)
	if (client[i].fd < 0)
	    break;

    if (i == client_size) {
	client_size = client_size ? client_size * 2 : MIN_CLIENTS_ALLOC;
	sz = sizeof(root_client_t) * client_size;
	client = (root_client_t *)realloc(client, sz);
	if (client == NULL) {
	    __pmNoMem("root_new_client", sz, PM_RECOV_ERR);
	    root_close_socket();
	    exit(1);
	}
	sz -= (sizeof(root_client_t) * i);
	memset(&client[i], 0, sz);
    }
    if (i >= nclients)
	nclients = i + 1;
    return i;
}

static void
root_delete_client(root_client_t *cp)
{
    int		i;

    for (i = 0; i < nclients; i++) {
	if (cp == &client[i])
	    break;
    }

    if (cp->fd != -1) {
	__pmFD_CLR(cp->fd, &connected_fds);
	__pmCloseSocket(cp->fd);
    }
    if (i >= nclients - 1)
	nclients = i;
    if (cp->fd == maximum_fd) {
	maximum_fd = (pmcd_fd > socket_fd) ? pmcd_fd : socket_fd;
	for (i = 0; i < nclients; i++) {
	    if (client[i].fd > maximum_fd)
		maximum_fd = client[i].fd;
	}
    }
    cp->fd = -1;
}

static root_client_t *
root_accept_client(void)
{
    int			i, fd;
    __pmSockLen		addrlen;

    i = root_new_client();
    addrlen = __pmSockAddrSize();
    if ((fd = __pmAccept(socket_fd, socket_addr, &addrlen)) < 0) {
	if (neterror() == EPERM) {
	    __pmNotifyErr(LOG_NOTICE, "root_accept_client(%d): %s\n",
			fd, osstrerror());
	    client[i].fd = -1;
	    root_delete_client(&client[i]);
	    return NULL;
	} else {
	    __pmNotifyErr(LOG_ERR, "root_accept_client(%d): accept: %s\n",
			fd, osstrerror());
	    root_close_socket();
	    exit(1);
	}
    }
    if (fd > maximum_fd)
	maximum_fd = fd;
    __pmFD_SET(fd, &connected_fds);

    client[i].fd = fd;
    return &client[i];
}

static void
root_check_new_client(__pmFdSet *fdset)
{
    root_client_t	*cp;
    int		sts;

    if ((cp = root_accept_client()) == NULL)
	return;	/* accept failed and no client added */

    /* send server capabilities */
    if ((sts = __pmdaSendRootPDUInfo(cp->fd, features, 0)) < 0) {
	__pmNotifyErr(LOG_ERR,
		"new_client: failed to ACK new client connection: %s\n",
		pmErrStr(sts));
	root_delete_client(cp);
    }
}

/*
 * Namespaced request for hostname as seen by the given process ID.
 */
static int
root_hostname(int pid, char *buffer, int *length)
{
#ifdef HAVE_SETNS
    static int utsfd = -1;
    char path[MAXPATHLEN];
    int fd, sts = 0;

    if (utsfd < 0) {
	if ((utsfd = open("/proc/self/ns/uts", O_RDONLY)) < 0)
	    return -oserror();
    }
    snprintf(path, sizeof(path), "/proc/%d/ns/uts", pid);
    if ((fd = open(path, O_RDONLY)) < 0)
	return -oserror();
    if (setns(fd, CLONE_NEWUTS) < 0) {
	sts = -oserror();
	close(fd);
	return sts;
    }
    close(fd);
    if ((gethostname(buffer, *length)) < 0) {
	sts = -oserror();
	*length = 0;
    }
    else {
	*length = strlen(buffer);
    }
    setns(utsfd, CLONE_NEWUTS);
    return sts;
#else
    return -EOPNOTSUPP;
#endif
}

static int
root_hostname_request(root_client_t *cp, void *pdu, int pdulen)
{
    container_t *container;
    char	name[MAXPATHLEN];
    char	buffer[MAXHOSTNAMELEN];
    int		sts, pid, length = 0, namelen;

    sts = __pmdaDecodeRootPDUContainer(pdu, pdulen, &pid, name, sizeof(name));
    if (sts < 0)
	return sts;
    namelen = sts;

    root_refresh_container_indom();
    if (sts > 0) {
	container = root_container_search(name);
	if (container) {
	    pid = container->pid;
	    sts = 0;
	} else {
	    if (pmDebug & DBG_TRACE_ATTR)
		__pmNotifyErr(LOG_DEBUG, "no container with name=%s\n", name);
	    sts = PM_ERR_NOCONTAINER;
	}
    }
    if (!sts) {
	length = sizeof(buffer);
	sts = root_hostname(pid, buffer, &length);
	if (pmDebug & DBG_TRACE_ATTR)
	    __pmNotifyErr(LOG_DEBUG, "pid=%d container=%s hostname=%s\n",
			pid, namelen ? name : "", sts < 0 ? "?" : buffer);
    }

    return __pmdaSendRootPDUContainer(cp->fd, PDUROOT_HOSTNAME,
			pid, buffer, length, sts);
}

static int
root_processid_request(root_client_t *cp, void *pdu, int pdulen)
{
    container_t *container;
    char	buffer[MAXPATHLEN], *name = &buffer[0];
    int		sts, pid;

    sts = __pmdaDecodeRootPDUContainer(pdu, pdulen, &pid, name, sizeof(buffer));
    if (sts < 0)
	return sts;
    if (sts == 0)
	return -EOPNOTSUPP;

    root_refresh_container_indom();
    if ((container = root_container_search(name)) != NULL) {
	pid = container->pid;
	sts = 0;
    } else {
	if (pmDebug & DBG_TRACE_ATTR)
	    __pmNotifyErr(LOG_DEBUG, "no container with name=%s\n", name);
	sts = PM_ERR_NOCONTAINER;
    }
    return __pmdaSendRootPDUContainer(cp->fd, PDUROOT_PROCESSID,
			pid, NULL, 0, sts);
}

static int
root_cgroupname_request(root_client_t *cp, void *pdu, int pdulen)
{
    container_t *container;
    char	name[MAXPATHLEN], *cgroup = NULL;
    int		sts, pid, length = 0;

    sts = __pmdaDecodeRootPDUContainer(pdu, pdulen, &pid, name, sizeof(name));
    if (sts < 0)
	return sts;
    if (sts == 0)
	return -EOPNOTSUPP;

    root_refresh_container_indom();
    if ((container = root_container_search(name)) == NULL) {
	if (pmDebug & DBG_TRACE_ATTR)
	    __pmNotifyErr(LOG_DEBUG, "no container with name=%s\n", name);
	sts = PM_ERR_NOCONTAINER;
    } else {
	sts = 0;
	pid = container->pid;
	cgroup = container->cgroup;
	length = strlen(container->cgroup);
	if (pmDebug & DBG_TRACE_ATTR)
	    __pmNotifyErr(LOG_DEBUG, "container %s cgroup=%s\n", name, cgroup);
    }
    return __pmdaSendRootPDUContainer(cp->fd, PDUROOT_CGROUPNAME,
			pid, cgroup, length, sts);
}

static int
root_recvpdu(int fd, __pmdaRootPDUHdr **hdr)
{
    static char		buffer[BUFSIZ];
    __pmdaRootPDUHdr	*pdu = (__pmdaRootPDUHdr *)buffer;
    int			bytes;

    if ((bytes = recv(fd, &buffer, sizeof(buffer), 0)) < 0) {
	__pmNotifyErr(LOG_ERR, "root_recvpdu: recv - %s\n", osstrerror());
	return bytes;
    }
    if (bytes == 0)	/* client disconnected */
	return 0;
    if (bytes < sizeof(__pmdaRootPDUHdr)) {
	__pmNotifyErr(LOG_ERR, "root_recvpdu: %d bytes too small\n", bytes);
	return -EINVAL;
    }
    if (pdu->version > ROOT_PDU_VERSION) {
	__pmNotifyErr(LOG_ERR, "root_recvpdu: client sent newer version (%d)\n",
			pdu->version);
	return -EOPNOTSUPP;
    }
    if (pdu->length < sizeof(__pmdaRootPDUHdr)) {
	__pmNotifyErr(LOG_ERR, "root_recvpdu: PDU length (%d) is too small\n",
			pdu->length);
	return -E2BIG;
    }
    if (pdu->status < 0) {
	__pmNotifyErr(LOG_ERR, "root_recvpdu: client sent bad status (%d)\n",
			pdu->status);
	return pdu->status;
    }
    *hdr = pdu;
    return bytes;
}

static void
root_handle_client_input(__pmFdSet *fds)
{
    __pmdaRootPDUHdr	*php = NULL;
    root_client_t	*cp;
    int			i, sts;

    for (i = 0; i < nclients; i++) {
	cp = &client[i];
	if (cp->fd == -1 || !__pmFD_ISSET(cp->fd, fds))
	    continue;

	if ((sts = root_recvpdu(cp->fd, &php)) <= 0) {
	    root_delete_client(cp);
	    continue;
	}

	switch (php->type) {
	case PDUROOT_HOSTNAME_REQ:
	    sts = root_hostname_request(cp, (void *)php, sts);
	    break;

	case PDUROOT_PROCESSID_REQ:
	    sts = root_processid_request(cp, (void *)php, sts);
	    break;

	case PDUROOT_CGROUPNAME_REQ:
	    sts = root_cgroupname_request(cp, (void *)php, sts);
	    break;

	/*
	 * We expect to add functionality here over time, e.g.:
	 * - PDU for (re)starting a PMDA & pass back fds;
	 * - authentication requests via SASL;
	 */

	default:
	    sts = PM_ERR_IPC;
	} 

	if (sts < 0) {
	    __pmNotifyErr(LOG_ERR, "bad protocol exchange (fd=%d)\n", cp->fd);
	    root_delete_client(cp);
	}
    }
}

/* Setup a select loop with af_unix socket fd & pmcd fds */
static void
root_main(pmdaInterface *dp)
{
    int         sts;
    int         maxfd, pmcd_fd;
    __pmFdSet	readable_fds;

    pmcd_fd = __pmdaInFd(dp);
    __pmFD_SET(pmcd_fd, &connected_fds);
    maximum_fd = (socket_fd > pmcd_fd) ? socket_fd : pmcd_fd;

    for (;;) {
	readable_fds = connected_fds;
	maxfd = maximum_fd + 1;

	sts = __pmSelectRead(maxfd, &readable_fds, NULL);
	if (sts > 0) {
	    if (__pmFD_ISSET(pmcd_fd, &readable_fds)) {
		if (pmDebug & DBG_TRACE_APPL0)
		    __pmNotifyErr(LOG_DEBUG, "pmcd request [fd=%d]", pmcd_fd);
		if (__pmdaMainPDU(dp) < 0)
		    exit(1);        /* it's fatal if we lose pmcd */
	    }
	    if (__pmFD_ISSET(socket_fd, &readable_fds))
		root_check_new_client(&readable_fds);
	    root_handle_client_input(&readable_fds);
	}
	else if (sts == -1 && neterror() != EINTR) {
	    __pmNotifyErr(LOG_ERR, "root_main select: %s\n", netstrerror());
	    break;
	}
    }
}

static void
root_check_user(void)
{
#ifdef HAVE_GETUID
    if (getuid() != 0) {
	__pmNotifyErr(LOG_ERR, "must be run as root\n");
	exit(1);
    }
#endif
}

static void
root_init(pmdaInterface *dp)
{
    root_check_user();
    root_setup_containers();
    root_container_search(NULL);	/* potentially costly early scan */
    root_setup_socket();
    atexit(root_close_socket);

    dp->version.any.fetch = root_fetch;
    dp->version.any.instance = root_instance;
    pmdaSetFetchCallBack(dp, root_fetchCallBack);
    root_indomtab[CONTAINERS_INDOM].it_indom = CONTAINERS_INDOM;
    pmdaSetFlags(dp, PMDA_EXT_FLAG_DIRECT);
    pmdaInit(dp, root_indomtab, INDOMTAB_SZ, root_metrictab, METRICTAB_SZ);
    pmdaCacheOp(INDOM(CONTAINERS_INDOM), PMDA_CACHE_CULL);
}

pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    { "socket", 1, 's', "PATH", "Unix domain socket file [default $PCP_TMP_DIR/pmcd/root.socket]" },
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions	opts = {
    .short_options = "D:d:l:s:?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    int			c, sep = __pmPathSeparator();
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    snprintf(helppath, sizeof(helppath), "%s%c" "root" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_6, pmProgname, ROOT, "root.log", helppath);

    while ((c = pmdaGetOptions(argc, argv, &opts, &dispatch)) != EOF) {
	switch (c) {
	case 's':
	    strncpy(socket_path, opts.optarg, sizeof(socket_path));
	    socket_path[sizeof(socket_path)-1] = '\0';
	    break;
	}
    }
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    pmdaConnect(&dispatch);
    root_init(&dispatch);
    root_main(&dispatch);
    exit(0);
}
