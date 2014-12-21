/*
 * PMCD privileged co-process (root) PMDA.
 *
 * Copyright (c) 2014 Red Hat.
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

static const int features = PDUROOT_FLAG_NS /* | ... */ ;

static pmdaIndom root_indomtab[NUM_INDOMS];
#define INDOM(x) (root_indomtab[x].it_indom)
#define INDOMTAB_SZ (sizeof(root_indomtab)/sizeof(root_indomtab[0]))

static pmdaMetric root_metrictab[] = {
    { NULL, { PMDA_PMID(0, CONTAINERS_DRIVER), PM_TYPE_STRING,
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

static container_driver_t drivers[] = {
    {
	.name		= "docker",
	.setup		= docker_setup,
	.indom_changed	= docker_indom_changed,
	.insts_refresh	= docker_insts_refresh,
	.value_refresh	= docker_value_refresh,
	.name_matching	= docker_name_matching,
    },
    { .name = NULL },
};

static void
root_setup_containers(void)
{
    container_driver_t *dp;

    for (dp = &drivers[0]; dp->name != NULL; dp++)
	dp->setup(dp);
}

static void
root_refresh_container_indom(void)
{
    int need_refresh = 0;
    container_driver_t *dp;
    pmInDom indom = INDOM(CONTAINERS_INDOM);

    for (dp = &drivers[0]; dp->name != NULL; dp++)
	need_refresh |= dp->indom_changed(dp);
    if (!need_refresh)
	return;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
    for (dp = &drivers[0]; dp->name != NULL; dp++)
	dp->insts_refresh(dp, indom);
}

static void
root_refresh_container_values(char *container, container_t *values)
{
    container_driver_t *dp;

    for (dp = &drivers[0]; dp->name != NULL; dp++)
	dp->value_refresh(dp, container, values);
}

int
root_container_search(const char *query)
{
    int sts, fuzzy, pid = -ESRCH, best = 0;
    char *name = NULL;
    container_t *cp;
    container_driver_t *dp;
    pmInDom indom = INDOM(CONTAINERS_INDOM);

    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(indom, sts, &name, (void **)&cp) || !cp)
	    continue;
	for (dp = &drivers[0]; dp->name != NULL; dp++) {
	    if ((fuzzy = dp->name_matching(dp, query, cp->name, name)) <= best)
		continue;
	    if (pmDebug & DBG_TRACE_ATTR)
		__pmNotifyErr(LOG_DEBUG, "container search: %s/%s (%d->%d)\n",
				query, name, best, fuzzy);
	    pid = cp->pid;
	    best = fuzzy;
	}
    }

    if (pmDebug & DBG_TRACE_ATTR) {
	if (best)
	    __pmNotifyErr(LOG_DEBUG, "found container: %s (%s/%d) pid=%d\n",
				name, query, best, pid);
	else
	    __pmNotifyErr(LOG_DEBUG, "container %s not matched\n", query);
    }

    return pid;
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
	root_refresh_container_values(name, cp);
	switch (idp->item) {
	case 0:		/* containers.driver */
	    atom->cp = cp->driver->name;
	    break;
	case 1:		/* containers.name */
	    atom->cp = cp->name;
	    break;
	case 2:		/* containers.pid */
	    atom->ul = cp->pid;
	    break;
	case 3:		/* containers.state.running */
	    atom->ul = (cp->status & CONTAINER_FLAG_RUNNING) != 0;
	    break;
	case 4:		/* containers.state.paused */
	    atom->ul = (cp->status & CONTAINER_FLAG_PAUSED) != 0;
	    break;
	case 5:		/* containers.state.restarting */
	    atom->ul = (cp->status & CONTAINER_FLAG_RESTARTING) != 0;
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

    snprintf(socket_path, sizeof(socket_path), "%s/pmcd/root.socket",
		pmGetConfig("PCP_TMP_DIR"));
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
root_accept_client(int requestfd)
{
    int			i, fd;
    __pmSockLen		addrlen;

    i = root_new_client();
    addrlen = __pmSockAddrSize();
    if ((fd = __pmAccept(requestfd, socket_addr, &addrlen)) < 0) {
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
root_check_new_client(__pmFdSet *fdset, int rfd, int family)
{
    if (__pmFD_ISSET(rfd, fdset)) {
	root_client_t	*cp;
	int		sts;

	if ((cp = root_accept_client(rfd)) == NULL)
	    return;	/* accept failed and no client added */

	/* send server capabilities */
	if ((sts = __pmdaSendRootPDUInfo(cp->fd, features, 0)) < 0) {
	    __pmNotifyErr(LOG_ERR,
		"new_client: failed to ACK new client connection: %s\n",
		pmErrStr(sts));
	    root_delete_client(cp);
	}
    }
}

static int
root_namespace_fds_request(root_client_t *cp, __pmdaRootPDUHdr *hdr)
{
    int		fdset[PMDA_NAMESPACE_COUNT] = { 0 };
    char	buffer[MAXPATHLEN], *name = &buffer[0];
    int		count = sizeof(buffer);
    int		flags = 0;
    int		pid = -1;
    int		sts;

    sts = __pmdaDecodeRootNameSpaceFdsReq((void*)hdr, &flags, &name, &count);
    if (sts < 0)
	return sts;

    /*
     * 1. refresh container_t instance domain in preparation
     * 2. match container name from PDU to an instance, via the
     *     container_driver_t match_names() interface.
     *     -> if not found, save error status to send back
     * 3. use pid and namespace flags to open namespace fds
     *     i.e. fd = open("/proc/PID/ns/FLAG") -> set of fds.
     *     -> if any errors, save error status to send back
     * 4. send back file descriptors to the requesting client
     * 5. close server process (local) open file descriptors.
     */
 
    root_refresh_container_indom();
    if ((pid = root_container_search(name)) < 0) {
	__pmNotifyErr(LOG_DEBUG, "no such container (name=%s)\n", name);
	/* error propogated back out via PDU .status */
    } else if ((sts = __pmdaOpenNameSpaceFds(flags, pid, fdset)) < 0) {
	__pmNotifyErr(LOG_ERR, "cannot open pid=%d namespace(s)\n", pid);
    }
    sts = __pmdaSendRootNameSpaceFds(cp->fd, pid, fdset, count, sts);
    __pmdaCloseNameSpaceFds(flags, fdset);
    return sts;
}

static __pmdaRootPDUHdr *
root_recvpdu(int fd)
{
    /*
     * TODO: recv the PDU (length is in the header), pass back a
     * pointer to the header.
     *
     * Check validity of the header in here too - status must be
     * zero (non-error), version must match, valid length, etc.
     *	    if (pdu->hdr.version > ROOT_PDU_VERSION)
     *		...
     *	    if (pdu->hdr.status != 0)
     *		...
     *
     * Note: AF_UNIX ancillary data never arrives, its only sent -
     * makes PDU handling for the server simpler than for clients!
     */

    return NULL;	/* NYI */
}

static void
root_handle_client_input(__pmFdSet *fds)
{
    __pmdaRootPDUHdr	*php;
    root_client_t	*cp;
    int			i, sts;

    for (i = 0; i < nclients; i++) {
	cp = &client[i];
	if (cp->fd == -1 || !__pmFD_ISSET(cp->fd, fds))
	    continue;

	if ((php = root_recvpdu(cp->fd)) == NULL) {
	    root_delete_client(cp);
	    continue;
	}

	switch (php->type) {
	case PDUROOT_NS_FDS_REQ:
	    sts = root_namespace_fds_request(cp, php);
	    break;

	/*
	 * We expect to add functionality here over time, e.g.:
	 * - container-name-to-cgroup-path lookups (pmdaproc and co).
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
            __pmServerAddNewClients(&readable_fds, root_check_new_client);
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
    __pmNotifyErr(LOG_DEBUG, "%s: root_init start\n", pmProgname);

    root_check_user();
    root_setup_containers();
    root_setup_socket();
    atexit(root_close_socket);

    dp->version.any.fetch = root_fetch;
    dp->version.any.instance = root_instance;
    pmdaSetFetchCallBack(dp, root_fetchCallBack);
    root_indomtab[CONTAINERS_INDOM].it_indom = CONTAINERS_INDOM;
    pmdaSetFlags(dp, PMDA_EXT_FLAG_DIRECT);
    pmdaInit(dp, root_indomtab, INDOMTAB_SZ, root_metrictab, METRICTAB_SZ);
    pmdaCacheOp(INDOM(CONTAINERS_INDOM), PMDA_CACHE_CULL);

    __pmNotifyErr(LOG_DEBUG, "%s: root_init end\n", pmProgname);
}

pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions	opts = {
    .short_options = "D:d:l:?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    snprintf(helppath, sizeof(helppath), "%s%c" "root" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_6, pmProgname, ROOT, "root.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    root_init(&dispatch);
    pmdaConnect(&dispatch);
    root_main(&dispatch);
    exit(0);
}
