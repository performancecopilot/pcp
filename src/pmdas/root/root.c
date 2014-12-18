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
#include "root.h"
#include "docker.h"
#include "domain.h"

#if 0
static char	socketpath[MAXPATHLEN];
static int	socketfd;
#endif

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
    container_driver_t *dp;
    int need_refresh = 0;
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

#if 0
#if defined(HAVE_STRUCT_SOCKADDR_UN)
static int
root_setup_socket(void)
{
    int			fd, sts;
    char		path[MAXPATHLEN];
    __pmSockAddr        *address;

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
	return -neterror();
    }
    if ((address = __pmSockAddrAlloc()) == NULL) {
	__pmNotifyErr(LOG_ERR, "setup: __pmSockAddrAlloc out of memory\n");
	return -ENOMEM;
    }
    snprintf(socketpath, sizeof(socketpath), "%s/pmcd/root.socket",
		pmGetConfig("PCP_TMP_DIR"));

    memcpy(path, socketpath, sizeof(path));	/* dirname copy */
    sts = __pmMakePath(dirname(path), S_IRWXU);
    if (sts < 0 && oserror() != EEXIST) {
	__pmNotifyErr(LOG_ERR, "setup: __pmMakePath failed on %s: %s\n",
			dirname(socketpath), osstrerror());
	memset(socketpath, 0, sizeof(socketpath));
	__pmSockAddrFree(address);
	return -oserror();
    }

    __pmSockAddrSetFamily(address, AF_UNIX);
    __pmSockAddrSetPath(address, socketpath);
    __pmServerSetLocalSocket(socketpath);

    sts = __pmBind(fd, (void *)address, __pmSockAddrSize());
    __pmSockAddrFree(address);
    if (sts < 0) {
	__pmNotifyErr(LOG_ERR, "setup: __pmBind failed: %s\n", netstrerror());
	unlink(socketpath);
	return -neterror();
    }

    socketfd = fd;
    return 0;
}
#else
static inline int root_setup_socket(void) { return 0; }
#endif

int
root_sendfd(int sock, int fd)
{
    struct msghdr hdr;
    struct iovec data;
    char cmsgbuf[CMSG_SPACE(sizeof(int))];
    char dummy = '*';

    data.iov_base = &dummy;
    data.iov_len = sizeof(dummy);

    memset(&hdr, 0, sizeof(hdr));
    hdr.msg_name = NULL;
    hdr.msg_namelen = 0;
    hdr.msg_iov = &data;
    hdr.msg_iovlen = 1;
    hdr.msg_flags = 0;

    hdr.msg_control = cmsgbuf;
    hdr.msg_controllen = CMSG_LEN(sizeof(int));

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&hdr);
    cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;

    *(int*)CMSG_DATA(cmsg) = fd;

    int n = sendmsg(sock, &hdr, 0);
    if (n == -1)
        __pmNotifyErr(LOG_ERR, "sendfd: sendmsg failed: %s\n", strerror(errno));

    return n;
}
#endif

static void
root_main(pmdaInterface *dp)
{
    /* TODO: custom PDU loop with AF_UNIX */
    pmdaMain(dp);
}

static void
root_check_user(void)
{
#ifndef IS_MINGW
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
    // root_setup_socket();

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
