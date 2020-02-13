/*
 * Copyright (c) 2012-2019 Red Hat.
 * Copyright (c) 1995-2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "libdefs.h"
#include <sys/stat.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

/*
 * Open an inet port to PMCD
 */

static void
__pmdaOpenSocket(char *sockname, int port, int family, int *infd, int *outfd)
{
    int			sts;
    int			sfd;
    struct servent	*service;
    __pmSockAddr	*myaddr;
    __pmSockLen		addrlen;
    __pmFdSet		rfds;
    int			one = 1;

    if (sockname != NULL) {	/* Translate port name to port num */
	service = getservbyname(sockname, NULL);
	if (service == NULL) {
	    pmNotifyErr(LOG_CRIT, "__pmdaOpenSocket: getservbyname(%s): %s\n", 
		    sockname, netstrerror());
	    exit(1);
	}
	port = service->s_port;
    }

    if (family != AF_INET6) {
	sfd = __pmCreateSocket();
	if (sfd < 0) {
	    pmNotifyErr(LOG_CRIT, "__pmdaOpenSocket: inet socket: %s\n",
			  netstrerror());
	    exit(1);
	}
    }
    else {
	sfd = __pmCreateIPv6Socket();
	if (sfd < 0) {
	    pmNotifyErr(LOG_CRIT, "__pmdaOpenSocket: ipv6 socket: %s\n",
			  netstrerror());
	    exit(1);
	}
    }
#ifndef IS_MINGW
    /*
     * allow port to be quickly re-used, e.g. when Install and PMDA already
     * installed, this becomes terminate and restart in a hurry ...
     */
    if (__pmSetSockOpt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
		(__pmSockLen)sizeof(one)) < 0) {
	pmNotifyErr(LOG_CRIT, "__pmdaOpenSocket: __pmSetSockOpt(reuseaddr): %s\n",
			netstrerror());
	exit(1);
    }
#else
    /* see MSDN tech note: "Using SO_REUSEADDR and SO_EXCLUSIVEADDRUSE" */
    if (__pmSetSockOpt(sfd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&one,
		(__pmSockLen)sizeof(one)) < 0) {
	pmNotifyErr(LOG_CRIT, "__pmdaOpenSocket: __pmSetSockOpt(excladdruse): %s\n",
			netstrerror());
	exit(1);
    }
#endif

    if ((myaddr =__pmSockAddrAlloc()) == NULL) {
	pmNotifyErr(LOG_CRIT, "__pmdaOpenSocket: sock addr alloc failed\n");
	exit(1);
    }
    __pmSockAddrInit(myaddr, family, INADDR_LOOPBACK, port);
    sts = __pmBind(sfd, (void *)myaddr, __pmSockAddrSize());
    if (sts < 0) {
	__pmSockAddrFree(myaddr);
	pmNotifyErr(LOG_CRIT, "__pmdaOpenSocket: bind: %s\n",
			netstrerror());
	exit(1);
    }

    sts = __pmListen(sfd, 1);	/* Max. 1 pending connection request (pmcd) */
    if (sts == -1) {
        __pmSockAddrFree(myaddr);
	pmNotifyErr(LOG_CRIT, "__pmdaOpenSocket: listen: %s\n",
			netstrerror());
	exit(1);
    }

    /* block here indefinitely, waiting for a connection */
    __pmFD_ZERO(&rfds);
    __pmFD_SET(sfd, &rfds);
    if ((sts = __pmSelectRead(sfd+1, &rfds, NULL)) != 1) {
        sts = (sts < 0) ? -neterror() : -EINVAL;
    }
    if (sts < 0) {
        __pmSockAddrFree(myaddr);
        pmNotifyErr(LOG_CRIT, "__pmdaOpenSocket: select: %s\n",
                        pmErrStr(sts));
        exit(1);
    }

    addrlen = __pmSockAddrSize();
    if ((*infd = __pmAccept(sfd, myaddr, &addrlen)) < 0) {
        __pmSockAddrFree(myaddr);
	pmNotifyErr(LOG_CRIT, "__pmdaOpenSocket: accept: %s\n",
			netstrerror());
	exit(1);
    }
    __pmCloseSocket(sfd);
    __pmSetSocketIPC(*infd);
    __pmSockAddrFree(myaddr);

    *outfd = *infd;
}

static void
__pmdaOpenInet(char *sockname, int port, int *infd, int *outfd)
{
    __pmdaOpenSocket(sockname, port, AF_INET, infd, outfd);
}

static void
__pmdaOpenIPv6(char *sockname, int port, int *infd, int *outfd)
{
    __pmdaOpenSocket(sockname, port, AF_INET6, infd, outfd);
}

#ifdef HAVE_STRUCT_SOCKADDR_UN
/*
 * Setup appropriate permissions for the Unix domain socket file
 *
 * pmcd must be able to talk to us on this channel, and it may be running
 * as an unprivileged user by the time the PMDA runs this code - so, need
 * to set permissions such that PCP_USER can access this file too.
 *
 * Note when run from dbpmda and/or QA these permissions might be fine to
 * stay as regular users, so we do not fail here and simply charge ahead.
 */
static void
socket_ownership(char *sockname)
{
    char		*username = pmGetConfig("PCP_USER");
    char		errmsg[128];
    struct passwd	*pw;
    int			sts;

    setoserror(0);
    if ((pw = getpwnam(username)) == NULL)
	pmNotifyErr(LOG_WARNING, "__pmdaOpenUnix: getpwnam(%s) failed: %s\n",
		username, pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));

    sts = chmod(sockname, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (sts == -1 && pmDebugOptions.libpmda)
	pmNotifyErr(LOG_WARNING, "__pmdaOpenUnix: chmod(%s,...) failed: %s\n",
			sockname, osstrerror());
    if (pw != NULL) {
	sts = chown(sockname, pw->pw_uid, pw->pw_gid);
	if (sts == -1 && pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_WARNING, "__pmdaOpenUnix: chown(%s, ...) failed: %s\n",
			    sockname, osstrerror());
    }
    else if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_WARNING, "__pmdaOpenUnix: chown(%s, ...) skipped\n",
			    sockname);
}

/*
 * Open a unix port to PMCD
 */
static void
__pmdaOpenUnix(char *sockname, int *infd, int *outfd)
{
    int			sts;
    int			sfd;
    int			len;
    __pmSockLen		addrlen;
    struct sockaddr_un	myaddr;
    struct sockaddr_un	from;

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd < 0) {
	pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: Unix domain socket: %s",
		     netstrerror());
	exit(1);
    }
    /* Sockets in the Unix domain are named pipes in the file system.
     * In case the socket is still hanging around, try to remove it.  If it
     * belonged to someone and is still open, they will still keep the
     * connection because the reference count is non-zero.
     */
    if ((sts = unlink(sockname)) == 0)
    	pmNotifyErr(LOG_WARNING, "__pmdaOpenUnix: Unix domain socket '%s' existed, unlinked it\n",
		     sockname);
    else if (sts < 0 && oserror() != ENOENT) {
	/* If can't unlink socket, give up.  We might end up with an
	 * unwanted connection to some other socket (from outer space)
	 */
	pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: Unlinking Unix domain socket '%s': %s\n",
		     sockname, osstrerror());
	exit(1);
    }
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sun_family = AF_UNIX;
    strcpy(myaddr.sun_path, sockname);
    len = (int)offsetof(struct sockaddr_un, sun_path) + (int)strlen(myaddr.sun_path);
    sts = bind(sfd, (struct sockaddr*) &myaddr, len);
    if (sts < 0) {
	pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: unix bind: %s\n",
			netstrerror());
	exit(1);
    }
    socket_ownership(sockname);

    sts = listen(sfd, 5);	/* Max. of 5 pending connection requests */
    if (sts == -1) {
	pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: unix listen: %s\n",
			netstrerror());
	exit(1);
    }
    addrlen = sizeof(from);
    /* block here, waiting for a connection */
    if ((*infd = accept(sfd, (struct sockaddr *)&from, &addrlen)) < 0) {
	pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: unix accept: %s\n",
			netstrerror());
	exit(1);
    }
    close(sfd);
    *outfd = *infd;
}
#else
static void
__pmdaOpenUnix(char *sockname, int *infd, int *outfd)
{
    pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: UNIX domain sockets unsupported\n");
    exit(1);
}
#endif

/*
 * Capture PMDA args using pmgetopts_r
 */

static char
pmdaIoTypeToOption(pmdaIoType io)
{
    switch(io) {
        case pmdaPipe:
	    return 'p';
        case pmdaInet:
	    return 'i';
        case pmdaIPv6:
	    return '6';
        case pmdaUnix:
	    return 'u';
        case pmdaUnknown:
        default:
	    break;
    }
    return '?';
}

/*
 * Backwards compatibility interface, short option support only.
 * We override username (-U) to preserve backward-compatibility
 * with the original pmdaGetOpt interface, which does not know
 * about that option (and uses of -U have been observed in the
 * wild).  Happily, pmdaGetOptions gives us a much more flexible
 * route forward.
 */

static int
username_override(int opt, pmdaOptions *opts)
{
    (void)opts;
    return opt == 'U';
}

int
pmdaGetOpt(int argc, char *const *argv, const char *optstring, pmdaInterface *dispatch, int *err)
{
    int sts;
    static pmdaOptions opts;

    opts.flags |= PM_OPTFLAG_POSIX;
    opts.short_options = optstring;
    opts.override = username_override;
    opts.errors = 0;

    sts = pmdaGetOptions(argc, argv, &opts, dispatch);

    optind = opts.optind;
    opterr = opts.opterr;
    optopt = opts.optopt;
    optarg = opts.optarg;
    *err += opts.errors;
    return sts;
}

/*
 * New, prefered interface - supports long and short options, allows
 * caller to select whether POSIX style options are required.  Also,
 * handles the common -U,--username option setting automatically.
 */
int
pmdaGetOptions(int argc, char *const *argv, pmdaOptions *opts, pmdaInterface *dispatch)
{
    int 	c = EOF;
    int		flag = 0;
    char	*endnum = NULL;
    pmdaExt     *pmda = NULL;

    if (dispatch->status != 0) {
	opts->errors++;
	return EOF;
    }

    if (!HAVE_ANY(dispatch->comm.pmda_interface)) {
	pmNotifyErr(LOG_CRIT, "pmdaGetOptions: "
		     "PMDA interface version %d not supported (domain=%d)",
		     dispatch->comm.pmda_interface, dispatch->domain);
	opts->errors++;
	return EOF;
    }

    pmda = dispatch->version.any.ext;

    /*
     * We only support one version of pmdaOptions structure so far - tell
     * the caller the struct size/fields we will be using (version zero),
     * in case they are of later version (newer PMDA, older library).
     *
     * The pmOptions and pmdaOptions structures share initial pmgetopts_r
     * fields, hence the cast below (and sharing of pmgetopt_r itself).
     *
     * So far, the only PMDA-specific field is from --username although,
     * of course, more may be added in the future (bumping version as we
     * go of course, and observing the version number the PMDA passes in).
     */
    opts->version = 0;

    while (!flag && ((c = pmgetopt_r(argc, argv, (pmOptions *)opts)) != EOF)) {
	int	sts;

	/* provide opportunity for overriding the general set of options */
	if (opts->override && opts->override(c, opts))
	    break;

	switch (c) {
	case 'd':
	    dispatch->domain = (int)strtol(opts->optarg, &endnum, 10);
	    if (*endnum != '\0') {
		pmprintf("%s: -d requires numeric domain number\n",
			 pmda->e_name);
		opts->errors++;
	    }
	    pmda->e_domain = dispatch->domain;
	    break;

	case 'D':
	    if ((sts = pmSetDebug(opts->optarg)) < 0) {
		pmprintf("%s: unrecognized debug options specification (%s)\n",
			pmda->e_name, opts->optarg);
		opts->errors++;
	    }
	    break;

	case 'h':	/* over-ride default help file */
	    pmda->e_helptext = strdup(opts->optarg);
	    break;

	case 'i':
	    if (pmda->e_io != pmdaUnknown && pmda->e_io != pmdaInet) {
		pmprintf("%s: -i option clashes with -%c option\n",
			    pmda->e_name, pmdaIoTypeToOption(pmda->e_io));
		opts->errors++;
	    } else {
		pmda->e_io = pmdaInet;
		pmda->e_port = (int)strtol(opts->optarg, &endnum, 10);
		if (*endnum != '\0')
		    pmda->e_sockname = opts->optarg;
	    }
	    break;

	case 'l':	/* over-ride default log file */
	    pmda->e_logfile = strdup(opts->optarg);
	    break;

	case 'p':
	    if (pmda->e_io != pmdaUnknown && pmda->e_io != pmdaPipe) {
		pmprintf("%s: -p option clashes with -%c option\n",
			    pmda->e_name, pmdaIoTypeToOption(pmda->e_io));
		opts->errors++;
	    } else {
		pmda->e_io = pmdaPipe;
	    }
	    break;

	case 'u':
	    if (pmda->e_io != pmdaUnknown && pmda->e_io != pmdaUnix) {
		pmprintf("%s: -u option clashes with -%c option\n",
			    pmda->e_name, pmdaIoTypeToOption(pmda->e_io));
		opts->errors++;
	    } else {
		pmda->e_io = pmdaUnix;
		pmda->e_sockname = opts->optarg;
	    }
	    break;

	case '6':
	    if (pmda->e_io != pmdaUnknown && pmda->e_io != pmdaIPv6) {
		pmprintf("%s: -6 option clashes with -%c option\n",
			    pmda->e_name, pmdaIoTypeToOption(pmda->e_io));
		opts->errors++;
	    } else {
		pmda->e_io = pmdaIPv6;
		pmda->e_port = (int)strtol(opts->optarg, &endnum, 10);
		if (*endnum != '\0')
		    pmda->e_sockname = opts->optarg;
	    }
	    break;

	case 'U':
	    opts->username = opts->optarg;
	    break;

	case '?':
	    opts->errors++;
	    break;

	default:
	    flag = 1;
	}
    }

    return c;
}

void
pmdaUsageMessage(pmdaOptions *opts)
{
    pmUsageMessage((pmOptions *)opts);
}

static __pmHashWalkState
pmdaHashNodeDelete(const __pmHashNode *tp, void *cp)
{
    (void)tp;
    (void)cp;
    return PM_HASH_WALK_DELETE_NEXT;
}

static void
pmdaHashDelete(__pmHashCtl *hashp)
{
    __pmHashWalkCB(pmdaHashNodeDelete, NULL, hashp);
    __pmHashClear(hashp);
}

/*
 * Recompute the hash table which maps metric PMIDs to metric table
 * offsets.  Provides an optimised lookup alternative when a direct
 * mapping is inappropriate or impossible.
 */
void
pmdaRehash(pmdaExt *pmda, pmdaMetric *metrics, int nmetrics)
{
    e_ext_t	*extp = (e_ext_t *)pmda->e_ext;
    __pmHashCtl *hashp = &extp->hashpmids;
    pmdaMetric	*metric;
    char	buf[32];
    int		m;

    pmda->e_direct = 0;
    pmda->e_metrics = metrics;
    pmda->e_nmetrics = nmetrics;

    pmdaHashDelete(hashp);
    for (m = 0; m < pmda->e_nmetrics; m++) {
	metric = &pmda->e_metrics[m];

	if (__pmHashAdd(metric->m_desc.pmid, metric, hashp) < 0) {
	    pmNotifyErr(LOG_WARNING, "pmdaRehash: PMDA %s: "
			"Hashed mapping for metrics disabled @ metric[%d] %s\n",
			pmda->e_name, m,
			pmIDStr_r(metric->m_desc.pmid, buf, sizeof(buf)));
	    break;
	}
    }
    if (m == pmda->e_nmetrics) {
	pmda->e_flags |= PMDA_EXT_FLAG_HASHED;
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "pmdaRehash: PMDA %s: successful rebuild\n",
			pmda->e_name);
    }
    else {
	pmda->e_flags &= ~PMDA_EXT_FLAG_HASHED;
	pmdaHashDelete(hashp);
    }
}

static void
pmdaDirect(pmdaExt *pmda, pmdaMetric *metrics, int nmetrics)
{
    __pmID_int	*pmidp;
    char	buf[20];
    int		m;

    pmda->e_direct = 1;
    for (m = 0; m < pmda->e_nmetrics; m++) {
	pmidp = (__pmID_int *)&pmda->e_metrics[m].m_desc.pmid;

	if (pmidp->item == m)
	    continue;

	pmda->e_direct = 0;
	if ((pmda->e_flags & PMDA_EXT_FLAG_DIRECT) || pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_WARNING, "pmdaDirect: PMDA %s: "
		"Direct mapping for metrics disabled @ metrics[%d] %s\n",
		pmda->e_name, m,
		pmIDStr_r(pmda->e_metrics[m].m_desc.pmid, buf, sizeof(buf)));
	break;
    }
}

void *
pmdaExtGetData(pmdaExt *pmda)
{
    e_ext_t	*extp = (e_ext_t *)pmda->e_ext;

    return extp->privdata;
}

void
pmdaExtSetData(pmdaExt *pmda, void *data)
{
    e_ext_t	*extp = (e_ext_t *)pmda->e_ext;

    if (pmDebugOptions.libpmda)
	pmNotifyErr(LOG_DEBUG, "pmdaExtSetData: data=%p", data);
    extp->privdata = data;
}

void
pmdaSetData(pmdaInterface *dispatch, void *data)
{
    if (HAVE_ANY(dispatch->comm.pmda_interface))
	pmdaExtSetData(dispatch->version.any.ext, data);
    else {
	pmNotifyErr(LOG_CRIT, "pmdaSetData: PMDA interface version %d not supported (domain=%d)",
		     dispatch->comm.pmda_interface, dispatch->domain);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
}

void
pmdaExtSetFlags(pmdaExt *pmda, int flags)
{
    if (pmDebugOptions.libpmda)
	pmNotifyErr(LOG_DEBUG, "pmdaExtSetFlags: flags=%x", flags);
    pmda->e_flags |= flags;
}

void
pmdaSetFlags(pmdaInterface *dispatch, int flags)
{
    if (HAVE_ANY(dispatch->comm.pmda_interface))
	pmdaExtSetFlags(dispatch->version.any.ext, flags);
    else {
	pmNotifyErr(LOG_CRIT, "pmdaSetFlags: PMDA interface version %d not supported (domain=%d)",
		     dispatch->comm.pmda_interface, dispatch->domain);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetCommFlags(pmdaInterface *dispatch, int flags)
{
    if (pmDebugOptions.libpmda)
	pmNotifyErr(LOG_DEBUG, "pmdaSetCommFlags: flags=%x", flags);
    dispatch->comm.flags |= flags;
}

/*
 * Open the help text file, check for direct mapping into the metric table
 * and whether a hash mapping has been requested.
 */

void
pmdaInit(pmdaInterface *dispatch, pmdaIndom *indoms, int nindoms,
	 pmdaMetric *metrics, int nmetrics)
{
    int		        m = 0;
    int                 i = 0;
    __pmInDom_int        *indomp = NULL;
    __pmInDom_int        *mindomp = NULL;
    __pmID_int	        *pmidp = NULL;
    pmdaExt	        *pmda = NULL;

    if (!HAVE_ANY(dispatch->comm.pmda_interface)) {
	pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA interface version %d not supported (domain=%d)",
		     dispatch->comm.pmda_interface, dispatch->domain);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
    pmda = dispatch->version.any.ext;

    if (dispatch->version.any.fetch == pmdaFetch &&
	pmda->e_fetchCallBack == (pmdaFetchCallBack)0) {
	pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA %s: using pmdaFetch() but fetch call back not set", pmda->e_name);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    /* parameter sanity checks */
    if (nmetrics < 0) {
	pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA %s: nmetrics (%d) should be non-negative", pmda->e_name, nmetrics);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
    if (nindoms < 0) {
	pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA %s: nindoms (%d) should be non-negative", pmda->e_name, nindoms);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
    if ((nmetrics == 0 && metrics != NULL) ||
        (nmetrics != 0 && metrics == NULL)){
	pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA %s: metrics (" PRINTF_P_PFX "%p) not consistent with nmetrics (%d)", pmda->e_name, metrics, nmetrics);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
    if ((nindoms == 0 && indoms != NULL) ||
        (nindoms != 0 && indoms == NULL)){
	pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA %s: indoms (" PRINTF_P_PFX "%p) not consistent with nindoms (%d)", pmda->e_name, indoms, nindoms);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    pmda->e_indoms = indoms;
    pmda->e_nindoms = nindoms;
    pmda->e_metrics = metrics;
    pmda->e_nmetrics = nmetrics;
    
    /* fix bit fields in indom for all instance domains */
    for (i = 0; i < pmda->e_nindoms; i++) {
	unsigned int domain = dispatch->domain;
	unsigned int serial = pmda->e_indoms[i].it_indom;
	pmda->e_indoms[i].it_indom = pmInDom_build(domain, serial);
    }

    /* fix bit fields in indom for all metrics */
    for (i = 0; i < pmda->e_nmetrics; i++) {
	if (pmda->e_metrics[i].m_desc.indom != PM_INDOM_NULL) {
	    unsigned int domain = dispatch->domain;
	    unsigned int serial = pmda->e_metrics[i].m_desc.indom;
	    pmda->e_metrics[i].m_desc.indom = pmInDom_build(domain, serial);
	}
    }

    /*
     * For each metric, check the instance domain serial number is valid
     */
    for (m = 0; m < pmda->e_nmetrics; m++) {
	if (pmda->e_metrics[m].m_desc.indom != PM_INDOM_NULL) {
	    mindomp = (__pmInDom_int *)&(pmda->e_metrics[m].m_desc.indom);
            if (pmda->e_nindoms > 0) {
		for (i = 0; i < pmda->e_nindoms; i++) {
		    indomp = (__pmInDom_int *)&(pmda->e_indoms[i].it_indom);
		    if (indomp->serial == mindomp->serial) {
			if (pmDebugOptions.libpmda) {
			    char	strbuf[20];
			    char	st2buf[20];
			    pmNotifyErr(LOG_DEBUG, 
				    "pmdaInit: PMDA %s: Metric %s(%d) matched to indom %s(%d)\n",
				    pmda->e_name,
				    pmIDStr_r(pmda->e_metrics[m].m_desc.pmid, strbuf, sizeof(strbuf)), m,
				    pmInDomStr_r(pmda->e_indoms[i].it_indom, st2buf, sizeof(st2buf)), i);
			}
			break;
		    }
		}
		if (i == pmda->e_nindoms) {
		    char	strbuf[20];
		    pmNotifyErr(LOG_CRIT, 
				 "pmdaInit: PMDA %s: Undefined instance domain serial (%d) specified in metric %s(%d)\n",
				 pmda->e_name, mindomp->serial, 
				 pmIDStr_r(pmda->e_metrics[m].m_desc.pmid, strbuf, sizeof(strbuf)), m);
		    dispatch->status = PM_ERR_GENERIC;
		    return;
		}
		    
	    }
	}
    }

    if (pmda->e_helptext != NULL) {
	pmda->e_help = pmdaOpenHelp(pmda->e_helptext);
	if (pmda->e_help < 0) {
	    pmNotifyErr(LOG_WARNING, "pmdaInit: PMDA %s: Unable to open help text file(s) from \"%s\": %s\n",
		    pmda->e_name, pmda->e_helptext, pmErrStr(pmda->e_help));
	}
	else if (pmDebugOptions.libpmda) {
	    pmNotifyErr(LOG_DEBUG, "pmdaInit: PMDA %s: help file %s opened\n", pmda->e_name, pmda->e_helptext);
	}
    }
    else {
	if (dispatch->version.two.text == pmdaText)
	    pmNotifyErr(LOG_WARNING, "pmdaInit: PMDA %s: No help text file specified for pmdaText", pmda->e_name); 
	else
	    if (pmDebugOptions.libpmda)
		pmNotifyErr(LOG_DEBUG, "pmdaInit: PMDA %s: No help text path specified", pmda->e_name);
    }

    /*
     * Stamp the correct domain number in each of the PMIDs
     */
    for (m = 0; m < pmda->e_nmetrics; m++) {
	pmidp = (__pmID_int *)&pmda->e_metrics[m].m_desc.pmid;
	pmidp->domain = dispatch->domain;
    }

    if (pmda->e_flags & PMDA_EXT_FLAG_HASHED)
	pmdaRehash(pmda, metrics, nmetrics);
    else
	pmdaDirect(pmda, metrics, nmetrics);

    if (pmDebugOptions.libpmda) {
    	pmNotifyErr(LOG_INFO, "name        = %s\n", pmda->e_name);
        pmNotifyErr(LOG_INFO, "domain      = %d\n", dispatch->domain);
        if (dispatch->comm.flags)
    	    pmNotifyErr(LOG_INFO, "comm flags  = %x\n", dispatch->comm.flags);
	pmNotifyErr(LOG_INFO, "ext flags  = %x\n", pmda->e_flags);
    	pmNotifyErr(LOG_INFO, "num metrics = %d\n", pmda->e_nmetrics);
    	pmNotifyErr(LOG_INFO, "num indom   = %d\n", pmda->e_nindoms);
    	pmNotifyErr(LOG_INFO, "metric map  = %s\n",
		(pmda->e_flags & PMDA_EXT_FLAG_HASHED) ? "hashed" :
		(pmda->e_direct ? "direct" : "linear"));
    }

    dispatch->status = pmda->e_status;
}

/*
 * Version exchange with pmcd via credentials PDU
 */

static int
__pmdaSetupPDU(int infd, int outfd, int flags, const char *agentname)
{
    __pmVersionCred	handshake;
    __pmCred		*credlist = NULL;
    __pmPDU		*pb;
    int			i, sts, pinpdu, vflag = 0;
    int			version = UNKNOWN_VERSION, credcount = 0, sender = 0;

    handshake.c_type = CVERSION;
    handshake.c_version = PDU_VERSION;
    handshake.c_flags = flags;
    if ((sts = __pmSendCreds(outfd, (int)getpid(), 1, (__pmCred *)&handshake)) < 0) {
	pmNotifyErr(LOG_CRIT, "__pmdaSetupPDU: PMDA %s send creds: %s\n", agentname, pmErrStr(sts));
	return -1;
    }

    if ((pinpdu = sts = __pmGetPDU(infd, ANY_SIZE, TIMEOUT_NEVER, &pb)) < 0) {
	pmNotifyErr(LOG_CRIT, "__pmdaSetupPDU: PMDA %s getting creds: %s\n", agentname, pmErrStr(sts));
	return -1;
    }

    if (sts == PDU_CREDS) {
	if ((sts = __pmDecodeCreds(pb, &sender, &credcount, &credlist)) < 0) {
	    pmNotifyErr(LOG_CRIT, "__pmdaSetupPDU: PMDA %s decode creds: %s\n", agentname, pmErrStr(sts));
	    __pmUnpinPDUBuf(pb);
	    return -1;
	}

	for (i = 0; i < credcount; i++) {
	    switch (credlist[i].c_type) {
	    case CVERSION:
		version = credlist[i].c_vala;
		vflag = 1;
		break;
	    default:
		pmNotifyErr(LOG_WARNING, "__pmdaSetupPDU: PMDA %s: unexpected creds PDU\n", agentname);
	    }
	}
	if (vflag) {
	    __pmSetVersionIPC(infd, version);
	    __pmSetVersionIPC(outfd, version);
	}
	if (credlist != NULL)
	    free(credlist);
    }
    else {
	char	strbuf[20];
	pmNotifyErr(LOG_CRIT, "__pmdaSetupPDU: PMDA %s: version exchange failure, got PDU type %s expecting PDU_CREDS)\n",
	    agentname,
	    __pmPDUTypeStr_r(sts, strbuf, sizeof(strbuf)));
    }

    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);

    return version;
}

/* 
 * Set up connection to PMCD 
 */

void
pmdaConnect(pmdaInterface *dispatch)
{
    pmdaExt	*pmda = NULL;
    int		sts, flags = dispatch->comm.flags;

    if (dispatch->version.any.ext == NULL ||
	(dispatch->version.any.ext->e_flags & PMDA_EXT_SETUPDONE) != PMDA_EXT_SETUPDONE) {
	pmNotifyErr(LOG_CRIT, "pmdaConnect: need to call pmdaDaemon() or pmdaDSO() first");
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    if ((dispatch->version.any.ext->e_flags & PMDA_EXT_CONNECTED) == PMDA_EXT_CONNECTED) {
	pmNotifyErr(LOG_CRIT, "pmdaConnect: called more than once");
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    if (!HAVE_ANY(dispatch->comm.pmda_interface)) {
	pmNotifyErr(LOG_CRIT, "pmdaConnect: PMDA interface version %d not supported (domain=%d)",
		     dispatch->comm.pmda_interface, dispatch->domain);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
    pmda = dispatch->version.any.ext;

    switch (pmda->e_io) {
	case pmdaPipe:
	case pmdaUnknown:		/* Default */

	    pmda->e_infd = fileno(stdin);
	    pmda->e_outfd = fileno(stdout);
#ifdef IS_MINGW
	    /* do not muck with \n in the PDU stream */
	    _setmode(pmda->e_infd, _O_BINARY);
	    _setmode(pmda->e_outfd, _O_BINARY);
#endif

	    if (pmDebugOptions.libpmda) {
	    	pmNotifyErr(LOG_DEBUG, "pmdaConnect: PMDA %s: opened pipe to pmcd, infd = %d, outfd = %d\n",
			     pmda->e_name, pmda->e_infd, pmda->e_outfd);
	    }
	    break;

	case pmdaInet:

	    __pmdaOpenInet(pmda->e_sockname, pmda->e_port, &(pmda->e_infd), 
			   &(pmda->e_outfd));

	    if (pmDebugOptions.libpmda) {
	    	pmNotifyErr(LOG_DEBUG, "pmdaConnect: PMDA %s: opened inet connection, infd = %d, outfd = %d\n",
			     pmda->e_name, pmda->e_infd, pmda->e_outfd);
	    }

	    break;

	case pmdaIPv6:

	    __pmdaOpenIPv6(pmda->e_sockname, pmda->e_port, &(pmda->e_infd), 
			   &(pmda->e_outfd));

	    if (pmDebugOptions.libpmda) {
	    	pmNotifyErr(LOG_DEBUG, "pmdaConnect: PMDA %s: opened ipv6 connection, infd = %d, outfd = %d\n",
			     pmda->e_name, pmda->e_infd, pmda->e_outfd);
	    }

	    break;

	case pmdaUnix:

	    __pmdaOpenUnix(pmda->e_sockname, &(pmda->e_infd), &(pmda->e_outfd));

	    if (pmDebugOptions.libpmda) {
	    	pmNotifyErr(LOG_DEBUG, "pmdaConnect: PMDA %s: Opened unix connection, infd = %d, outfd = %d\n",
			     pmda->e_name, pmda->e_infd, pmda->e_outfd);
	    }

	    break;
	default:
	    pmNotifyErr(LOG_CRIT, "pmdaConnect: PMDA %s: Illegal iotype: %d\n", pmda->e_name, pmda->e_io);
	    exit(1);
    }

    sts = __pmdaSetupPDU(pmda->e_infd, pmda->e_outfd, flags, pmda->e_name);
    if (sts < 0)
	dispatch->status = sts;
    else {
	dispatch->comm.pmapi_version = (unsigned int)sts;
	pmda->e_flags |= PMDA_EXT_CONNECTED;
    }
}

/*
 * Initialise the pmdaExt and pmdaInterface structures for a daemon or DSO PMDA.
 */

static void
__pmdaSetup(pmdaInterface *dispatch, int version, const char *name)
{
    pmdaExt	*pmda = NULL;
    e_ext_t	*extp;

    if (!HAVE_ANY(version)) {
	pmNotifyErr(LOG_CRIT, "__pmdaSetup: %s PMDA: interface version %d not supported (domain=%d)",
		     name, version, dispatch->domain);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    pmda = (pmdaExt *)calloc(1, sizeof(pmdaExt));
    if (pmda == NULL) {
	pmNotifyErr(LOG_ERR, 
		     "%s: Unable to allocate memory for pmdaExt structure (%d bytes)",
		     name, (int)sizeof(pmdaExt));
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    dispatch->status = 0;

    dispatch->comm.pmda_interface = version;
    dispatch->comm.pmapi_version = PMAPI_VERSION;
    dispatch->comm.flags = 0;

    if (HAVE_V_SEVEN(version)) {
	dispatch->version.seven.label = pmdaLabel;
    }
    if (HAVE_V_SIX(version)) {
	dispatch->version.six.attribute = pmdaAttribute;
    }
    if (HAVE_V_FOUR(version)) {
	dispatch->version.four.pmid = pmdaPMID;
	dispatch->version.four.name = pmdaName;
	dispatch->version.four.children = pmdaChildren;
    }
    dispatch->version.any.profile = pmdaProfile;
    dispatch->version.any.fetch = pmdaFetch;
    dispatch->version.any.desc = pmdaDesc;
    dispatch->version.any.instance = pmdaInstance;
    dispatch->version.any.text = pmdaText;
    dispatch->version.any.store = pmdaStore;
    dispatch->version.any.ext = pmda;

    pmda->e_name = name;
    pmda->e_infd = -1;
    pmda->e_outfd = -1;
    pmda->e_port = -1;
    pmda->e_singular = -1;
    pmda->e_ordinal = -1;
    pmda->e_domain = dispatch->domain;
    pmda->e_help = -1;
    pmda->e_io = pmdaUnknown;
    pmda->e_ext = (void *)dispatch;
    pmda->e_flags |= PMDA_EXT_SETUPDONE;

    extp = (e_ext_t *)calloc(1, sizeof(*extp));
    if (extp == NULL) {
	pmNotifyErr(LOG_ERR, 
		     "%s: Unable to allocate memory for e_ext_t structure (%d bytes)",
		     name, (int)sizeof(*extp));
	free(pmda);
	dispatch->version.any.ext = NULL;
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
    extp->dispatch = dispatch;
    pmda->e_ext = (void *)extp;
    pmdaSetResultCallBack(dispatch, __pmFreeResultValues);
}

/*
 * Initialise the pmdaExt and pmdaInterface structures for a daemon
 * also set some globals.
 */

void
pmdaDaemon(pmdaInterface *dispatch, int version, const char *name, int domain,
	   const char *logfile, const char *helptext)
{
    pmdaExt	*pmda;

    dispatch->domain = domain;
    __pmdaSetup(dispatch, version, name);

    if (dispatch->status < 0)
	return;

    pmda = dispatch->version.any.ext;
    pmda->e_logfile = (logfile == NULL ? NULL : strdup(logfile));
    pmda->e_helptext = (helptext == NULL ? NULL : strdup(helptext));

    __pmSetInternalState(PM_STATE_PMCS);
}

/*
 * Initialise the pmdaExt and pmdaInterface structures for a DSO,
 * also set some globals.
 */

void
pmdaDSO(pmdaInterface *dispatch, int version, char *name, char *helptext)
{
    __pmdaSetup(dispatch, version, name);

    if (dispatch->status < 0)
	return;

    dispatch->version.any.ext->e_helptext = (helptext == NULL ? NULL : strdup(helptext));
}

/*
 * Redirect stderr to the log file
 */

void
pmdaOpenLog(pmdaInterface *dispatch)
{
    int c;

    if (dispatch->status < 0)
	return;

    if (pmDebugOptions.libpmda && pmDebugOptions.desperate)
	setlinebuf(stderr);

    pmOpenLog(dispatch->version.any.ext->e_name, 
		dispatch->version.any.ext->e_logfile, stderr, &c);
}

/*
 * pmdaInterface was moved ... fix e_ext back pointer
 */
void
pmdaInterfaceMoved(pmdaInterface *dispatch)
{
    if (dispatch->version.any.ext != NULL) {
	if (dispatch->version.any.ext->e_ext != NULL) {
	    e_ext_t	*extp;
	    extp = (e_ext_t *)dispatch->version.any.ext->e_ext;
	    extp->dispatch = dispatch;
	}
    }
}
