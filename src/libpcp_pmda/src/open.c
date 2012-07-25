/*
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
#include "impl.h"
#include "pmda.h"
#include "libdefs.h"
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

void
__pmdaOpenInet(char *sockname, int myport, int *infd, int *outfd)
{
    int			sts;
    int			sfd;
    struct sockaddr_in	myaddr;
    struct sockaddr_in	from;
    struct servent	*service;
    mysocklen_t		addrlen;
    int			one = 1;

    if (sockname != NULL) {	/* Translate port name to port num */
	service = getservbyname(sockname, NULL);
	if (service == NULL) {
	    __pmNotifyErr(LOG_CRIT, "__pmdaOpenInet: getservbyname(%s): %s\n", 
		    sockname, netstrerror());
	    exit(1);
	}
	myport = service->s_port;
    }

    sfd = __pmCreateSocket();
    if (sfd < 0) {
	__pmNotifyErr(LOG_CRIT, "__pmdaOpenInet: inet socket: %s\n",
			netstrerror());
	exit(1);
    }
#ifndef IS_MINGW
    /*
     * allow port to be quickly re-used, e.g. when Install and PMDA already
     * installed, this becomes terminate and restart in a hurry ...
     */
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
		(mysocklen_t)sizeof(one)) < 0) {
	__pmNotifyErr(LOG_CRIT, "__pmdaOpenInet: setsockopt(reuseaddr): %s\n",
			netstrerror());
	exit(1);
    }
#else
    /* see MSDN tech note: "Using SO_REUSEADDR and SO_EXCLUSIVEADDRUSE" */
    if (setsockopt(sfd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&one,
		(mysocklen_t)sizeof(one)) < 0) {
	__pmNotifyErr(LOG_CRIT, "__pmdaOpenInet: setsockopt(excladdruse): %s\n",
			netstrerror());
	exit(1);
    }
#endif

    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(myport);
    sts = bind(sfd, (struct sockaddr*) &myaddr, sizeof(myaddr));
    if (sts < 0) {
	__pmNotifyErr(LOG_CRIT, "__pmdaOpenInet: inet bind: %s\n",
			netstrerror());
	exit(1);
    }

    sts = listen(sfd, 5);	/* Max. of 5 pending connection requests */
    if (sts == -1) {
	__pmNotifyErr(LOG_CRIT, "__pmdaOpenInet: inet listen: %s\n",
			netstrerror());
	exit(1);
    }
    addrlen = sizeof(from);
    /* block here, waiting for a connection */
    if ((*infd = accept(sfd, (struct sockaddr *)&from, &addrlen)) < 0) {
	__pmNotifyErr(LOG_CRIT, "__pmdaOpenInet: inet accept: %s\n",
			netstrerror());
	exit(1);
    }
    __pmCloseSocket(sfd);
    __pmSetSocketIPC(*infd);
    *outfd = *infd;
}

#if !defined(IS_MINGW)
/*
 * Open a unix port to PMCD
 */

void
__pmdaOpenUnix(char *sockname, int *infd, int *outfd)
{
    int			sts;
    int			sfd;
    int			len;
    mysocklen_t		addrlen;
    struct sockaddr_un	myaddr;
    struct sockaddr_un	from;

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd < 0) {
	__pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: Unix domain socket: %s",
		     netstrerror());
	exit(1);
    }
    /* Sockets in the Unix domain are named pipes in the file system.
     * In case the socket is still hanging around, try to remove it.  If it
     * belonged to someone and is still open, they will still keep the
     * connection because the reference count is non-zero.
     */
    if ((sts = unlink(sockname)) == 0)
    	__pmNotifyErr(LOG_WARNING, "__pmdaOpenUnix: Unix domain socket '%s' existed, unlinked it\n",
		     sockname);
    else if (sts < 0 && oserror() != ENOENT) {
	/* If can't unlink socket, give up.  We might end up with an
	 * unwanted connection to some other socket (from outer space)
	 */
	__pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: Unlinking Unix domain socket '%s': %s\n",
		     sockname, osstrerror());
	exit(1);
    }
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sun_family = AF_UNIX;
    strcpy(myaddr.sun_path, sockname);
    len = (int)offsetof(struct sockaddr_un, sun_path) + (int)strlen(myaddr.sun_path);
    sts = bind(sfd, (struct sockaddr*) &myaddr, len);
    if (sts < 0) {
	__pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: unix bind: %s\n",
			netstrerror());
	exit(1);
    }

    sts = listen(sfd, 5);	/* Max. of 5 pending connection requests */
    if (sts == -1) {
	__pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: unix listen: %s\n",
			netstrerror());
	exit(1);
    }
    addrlen = sizeof(from);
    /* block here, waiting for a connection */
    if ((*infd = accept(sfd, (struct sockaddr *)&from, &addrlen)) < 0) {
	__pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: unix accept: %s\n",
			netstrerror());
	exit(1);
    }
    close(sfd);
    *outfd = *infd;
}

#else	/* MINGW */
void
__pmdaOpenUnix(char *sockname, int *infd, int *outfd)
{
    __pmNotifyErr(LOG_CRIT, "__pmdaOpenUnix: Not supported on Windows");
    exit(1);
}
#endif

/*
 * capture PMDA args from getopts 
 */

int 
pmdaGetOpt(int argc, char *const *argv, const char *optstring, pmdaInterface *dispatch, 
	   int *err)
{
    int 	c;
    int		flag = 0;
    int		sts;
    char	*endnum = NULL;
    pmdaExt     *pmda = NULL;

    if (dispatch->status != 0) {
	(*err)++;
	return EOF;
    }

    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
	pmda = dispatch->version.four.ext;
    else if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	pmda = dispatch->version.two.ext;
    else {
	__pmNotifyErr(LOG_CRIT, "pmdaGetOpt: PMDA interface version %d not supported (domain=%d)",
		     dispatch->comm.pmda_interface, dispatch->domain);
	(*err)++;
	return EOF;
    }

    while (!flag && ((c = getopt(argc, argv, optstring)) != EOF)) {
    	switch (c) {
	    case 'd':
		dispatch->domain = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, "%s: -d requires numeric domain number\n",
		    	    pmda->e_name);
		    (*err)++;
		}
		pmda->e_domain = dispatch->domain;
		break;

	    case 'D':
		sts = __pmParseDebug(optarg);
		if (sts < 0) {
		    fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
			pmda->e_name, optarg);
		    (*err)++;
		}
		else
		    pmDebug |= sts;
		break;
	    
	    case 'h':
		pmda->e_helptext = optarg;
		break;

	    case 'i':
		if (pmda->e_io != pmdaUnknown && pmda->e_io != pmdaInet) {
		    fprintf(stderr, "%s: -i option clashes with -%c option\n",
			    pmda->e_name, (pmda->e_io == pmdaUnix) ? 'u' : 'p');
		    (*err)++;
		    break;
		}
		pmda->e_io = pmdaInet;
		pmda->e_port = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0')
		    pmda->e_sockname = optarg;
		break;

	    case 'l':
		/* over-ride default log file */
		pmda->e_logfile = optarg;
		break;

	    case 'p':
		if (pmda->e_io != pmdaUnknown && pmda->e_io != pmdaPipe) {
		    fprintf(stderr, "%s: -p option clashes with -%c option\n",
			    pmda->e_name, (pmda->e_io == pmdaUnix) ? 'u' : 'i');
		    (*err)++;
		    break;
		}
		pmda->e_io = pmdaPipe;
		break;

	    case 'u':
		if (pmda->e_io != pmdaUnknown && pmda->e_io != pmdaUnix) {
		    fprintf(stderr, "%s: -u option clashes with -%c option\n",
			    pmda->e_name, (pmda->e_io == pmdaPipe) ? 'p' : 'i');
		    (*err)++;
		    break;
		}
		pmda->e_io = pmdaUnix;
		pmda->e_sockname = optarg;
		break;

	    case '?':
		(*err)++;
		break;

	    default:
	    	flag = 1;
	}
    }

    return c;
}

/*
 * open the help text file and check for direct mapping into the metric table
 */

void
pmdaInit(pmdaInterface *dispatch, pmdaIndom *indoms, int nindoms, pmdaMetric *metrics, 
	 int nmetrics)
{
    int		        m = 0;
    int                 i = 0;
    __pmInDom_int        *indomp = NULL;
    __pmInDom_int        *mindomp = NULL;
    __pmID_int	        *pmidp = NULL;
    pmdaExt	        *pmda = NULL;

    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
	pmda = dispatch->version.four.ext;
    else if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	pmda = dispatch->version.two.ext;
    else {
	__pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA interface version %d not supported (domain=%d)",
		     dispatch->comm.pmda_interface, dispatch->domain);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    if (((HAVE_V_FOUR(dispatch->comm.pmda_interface) &&
	  dispatch->version.four.fetch == pmdaFetch) ||
	 (HAVE_V_TWO(dispatch->comm.pmda_interface) &&
	  dispatch->version.two.fetch == pmdaFetch)) &&
	pmda->e_fetchCallBack == (pmdaFetchCallBack)0) {
	__pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA %s: using pmdaFetch() but fetch call back not set", pmda->e_name);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    pmda->e_indoms = indoms;
    pmda->e_nindoms = nindoms;
    pmda->e_metrics = metrics;
    pmda->e_nmetrics = nmetrics;

    /* parameter sanity checks */
    if (nmetrics < 0) {
	__pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA %s: nmetrics (%d) should be non-negative", pmda->e_name, nmetrics);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
    if (nindoms < 0) {
	__pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA %s: nindoms (%d) should be non-negative", pmda->e_name, nindoms);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
    if ((nmetrics == 0 && metrics != NULL) ||
        (nmetrics != 0 && metrics == NULL)){
	__pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA %s: metrics not consistent with nmetrics", pmda->e_name);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
    if ((nindoms == 0 && indoms != NULL) ||
        (nindoms != 0 && indoms == NULL)){
	__pmNotifyErr(LOG_CRIT, "pmdaInit: PMDA %s: indoms not consistent with nindoms", pmda->e_name);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
    

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
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_LIBPMDA) {
			    char	strbuf[20];
			    char	st2buf[20];
			    __pmNotifyErr(LOG_DEBUG, 
				    "pmdaInit: PMDA %s: Metric %s(%d) matched to indom %s(%d)\n",
				    pmda->e_name,
				    pmIDStr_r(pmda->e_metrics[m].m_desc.pmid, strbuf, sizeof(strbuf)), m,
				    pmInDomStr_r(pmda->e_indoms[i].it_indom, st2buf, sizeof(st2buf)), i);
			}
#endif
			break;
		    }
		}
		if (i == pmda->e_nindoms) {
		    char	strbuf[20];
		    __pmNotifyErr(LOG_CRIT, 
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
	    __pmNotifyErr(LOG_WARNING, "pmdaInit: PMDA %s: Unable to open help text file \"%s\": %s\n",
		    pmda->e_name, pmda->e_helptext, pmErrStr(pmda->e_help));
	}
#ifdef PCP_DEBUG
	else if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "pmdaInit: PMDA %s: help file %s opened\n", pmda->e_name, pmda->e_helptext);
	}
#endif
    }
    else {
	if (dispatch->version.two.text == pmdaText)
	    __pmNotifyErr(LOG_WARNING, "pmdaInit: PMDA %s: No help text file specified", pmda->e_name); 
#ifdef PCP_DEBUG
	else
	    if (pmDebug & DBG_TRACE_LIBPMDA)
		__pmNotifyErr(LOG_DEBUG, "pmdaInit: PMDA %s: No help text path specified", pmda->e_name);
#endif
    }

    pmda->e_direct = 1;

    for (m = 0; m < pmda->e_nmetrics; m++)
    {
    	pmidp = (__pmID_int *)&pmda->e_metrics[m].m_desc.pmid;
	pmidp->domain = dispatch->domain;

	if (pmda->e_direct && pmidp->item != m) {
	    pmda->e_direct = 0;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		char	strbuf[20];
		__pmNotifyErr(LOG_WARNING, "pmdaInit: PMDA %s: Direct mapping for metrics disabled @ metrics[%d] %s\n", pmda->e_name, m, pmIDStr_r(pmda->e_metrics[m].m_desc.pmid, strbuf, sizeof(strbuf)));
	    }
#endif
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LIBPMDA)
    {
    	__pmNotifyErr(LOG_INFO, "name        = %s\n", pmda->e_name);
	__pmNotifyErr(LOG_INFO, "domain      = %d\n", dispatch->domain);
    	__pmNotifyErr(LOG_INFO, "num metrics = %d\n", pmda->e_nmetrics);
    	__pmNotifyErr(LOG_INFO, "num indom   = %d\n", pmda->e_nindoms);
    	__pmNotifyErr(LOG_INFO, "direct map  = %d\n", pmda->e_direct);
    }
#endif

    dispatch->status = pmda->e_status;
}

/*
 * version exchange with pmcd via credentials PDU
 */

int
__pmdaSetupPDU(int infd, int outfd, char *agentname)
{
    __pmCred	handshake[1];
    __pmCred	*credlist = NULL;
    __pmPDU	*pb;
    int		i, sts, vflag = 0;
    int		version = UNKNOWN_VERSION, credcount = 0, sender = 0;
    int		pinpdu;

    handshake[0].c_type = CVERSION;
    handshake[0].c_vala = PDU_VERSION;
    handshake[0].c_valb = 0;
    handshake[0].c_valc = 0;
    if ((sts = __pmSendCreds(outfd, (int)getpid(), 1, handshake)) < 0) {
	__pmNotifyErr(LOG_CRIT, "__pmdaSetupPDU: PMDA %s send creds: %s\n", agentname, pmErrStr(sts));
	return -1;
    }

    if ((pinpdu = sts = __pmGetPDU(infd, ANY_SIZE, TIMEOUT_DEFAULT, &pb)) < 0) {
	__pmNotifyErr(LOG_CRIT, "__pmdaSetupPDU: PMDA %s getting creds: %s\n", agentname, pmErrStr(sts));
	return -1;
    }

    if (sts == PDU_CREDS) {
	if ((sts = __pmDecodeCreds(pb, &sender, &credcount, &credlist)) < 0) {
	    __pmNotifyErr(LOG_CRIT, "__pmdaSetupPDU: PMDA %s decode creds: %s\n", agentname, pmErrStr(sts));
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
		__pmNotifyErr(LOG_WARNING, "__pmdaSetupPDU: PMDA %s: unexpected creds PDU\n", agentname);
	    }
	}
	if (vflag) {
	    __pmSetVersionIPC(infd, version);
	    __pmSetVersionIPC(outfd, version);
	}
	if (credlist != NULL)
	    free(credlist);
    }
    else
	__pmNotifyErr(LOG_CRIT, "__pmdaSetupPDU: PMDA %s: version exchange failure\n", agentname);

    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);

    return version;
}

/* 
 * set up connection to PMCD 
 */

void
pmdaConnect(pmdaInterface *dispatch)
{
    pmdaExt	*pmda = NULL;
    int		sts;

    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
	pmda = dispatch->version.four.ext;
    else if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	pmda = dispatch->version.two.ext;
    else {
	__pmNotifyErr(LOG_CRIT, "pmdaConnect: PMDA interface version %d not supported (domain=%d)",
		     dispatch->comm.pmda_interface, dispatch->domain);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    switch (pmda->e_io) {
	case pmdaPipe:
	case pmdaUnknown:		/* Default */

	    pmda->e_infd = fileno(stdin);
	    pmda->e_outfd = fileno(stdout);

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
	    	__pmNotifyErr(LOG_DEBUG, "pmdaConnect: PMDA %s: opened pipe to pmcd, infd = %d, outfd = %d\n",
			     pmda->e_name, pmda->e_infd, pmda->e_outfd);
	    }
#endif
	    break;

	case pmdaInet:

	    __pmdaOpenInet(pmda->e_sockname, pmda->e_port, &(pmda->e_infd), 
	    		  &(pmda->e_outfd));

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
	    	__pmNotifyErr(LOG_DEBUG, "pmdaConnect: PMDA %s: opened inet connection, infd = %d, outfd = %d\n",
			     pmda->e_name, pmda->e_infd, pmda->e_outfd);
	    }
#endif

	    break;

	case pmdaUnix:

	    __pmdaOpenUnix(pmda->e_sockname, &(pmda->e_infd), &(pmda->e_outfd));

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
	    	__pmNotifyErr(LOG_DEBUG, "pmdaConnect: PMDA %s: Opened unix connection, infd = %d, outfd = %d\n",
			     pmda->e_name, pmda->e_infd, pmda->e_outfd);
	    }
#endif

	    break;
	default:
	    __pmNotifyErr(LOG_CRIT, "pmdaConnect: PMDA %s: Illegal iotype: %d\n", pmda->e_name, pmda->e_io);
	    exit(1);
    }

    if (HAVE_V_FOUR(dispatch->comm.pmda_interface) ||
        HAVE_V_TWO(dispatch->comm.pmda_interface)) {
	if ((sts = __pmdaSetupPDU(pmda->e_infd, pmda->e_outfd, pmda->e_name)) < 0)
	    dispatch->status = sts;
	else
	    dispatch->comm.pmapi_version = (unsigned int)sts;
    }
}

/*
 * initialise the pmdaExt and pmdaInterface structures for a daemon or DSO PMDA.
 */

void
__pmdaSetup(pmdaInterface *dispatch, int version, char *name)
{
    pmdaExt	*pmda = NULL;
    e_ext_t	*extp;

    if (!HAVE_V_FOUR(version) && !HAVE_V_TWO(version)) {
	__pmNotifyErr(LOG_CRIT, "__pmdaSetup: %s PMDA: interface version %d not supported (domain=%d)",
		     name, version, dispatch->domain);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    pmda = (pmdaExt *)malloc(sizeof(pmdaExt));
    if (pmda == NULL) {
	__pmNotifyErr(LOG_ERR, 
		     "%s: Unable to allocate memory for pmdaExt structure (%d bytes)",
		     name, (int)sizeof(pmdaExt));
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    dispatch->status = 0;

    dispatch->comm.pmda_interface = version;
    dispatch->comm.pmapi_version = PMAPI_VERSION;
    dispatch->comm.flags = 0;

    if (HAVE_V_FOUR(version)) {
	dispatch->version.four.profile = pmdaProfile;
	dispatch->version.four.fetch = pmdaFetch;
	dispatch->version.four.desc = pmdaDesc;
	dispatch->version.four.instance = pmdaInstance;
	dispatch->version.four.text = pmdaText;
	dispatch->version.four.store = pmdaStore;
	dispatch->version.four.pmid = pmdaPMID;
	dispatch->version.four.name = pmdaName;
	dispatch->version.four.children = pmdaChildren;
	dispatch->version.four.ext = pmda;
    }
    else {
	dispatch->version.two.profile = pmdaProfile;
	dispatch->version.two.fetch = pmdaFetch;
	dispatch->version.two.desc = pmdaDesc;
	dispatch->version.two.instance = pmdaInstance;
	dispatch->version.two.text = pmdaText;
	dispatch->version.two.store = pmdaStore;
	dispatch->version.two.ext = pmda;
    }

    pmda->e_sockname = NULL;
    pmda->e_name = name;
    pmda->e_logfile = NULL;
    pmda->e_helptext = NULL;
    pmda->e_status = 0;
    pmda->e_infd = -1;
    pmda->e_outfd = -1;
    pmda->e_port = -1;
    pmda->e_singular = -1;
    pmda->e_ordinal = -1;
    pmda->e_direct = 0;
    pmda->e_domain = dispatch->domain;
    pmda->e_nmetrics = 0;
    pmda->e_nindoms = 0;
    pmda->e_help = -1;
    pmda->e_prof = NULL;
    pmda->e_io = pmdaUnknown;
    pmda->e_indoms = NULL;
    pmda->e_idp = NULL;
    pmda->e_metrics = NULL;
    pmda->e_ext = (void *)dispatch;

    extp = (e_ext_t *)malloc(sizeof(*extp));
    if (extp == NULL) {
	__pmNotifyErr(LOG_ERR, 
		     "%s: Unable to allocate memory for e_ext_t structure (%d bytes)",
		     name, (int)sizeof(*extp));
	free(pmda);
	if (HAVE_V_FOUR(version))
	    dispatch->version.four.ext = NULL;
	else
	    dispatch->version.two.ext = NULL;
	dispatch->status = PM_ERR_GENERIC;
	return;
    }
    extp->pmda_interface = version;
    extp->res = NULL;
    extp->maxnpmids = 0;
    pmda->e_ext = (void *)extp;

    pmdaSetResultCallBack(dispatch, __pmFreeResultValues);
    pmdaSetFetchCallBack(dispatch, (pmdaFetchCallBack)0);
    pmdaSetCheckCallBack(dispatch, (pmdaCheckCallBack)0);
    pmdaSetDoneCallBack(dispatch, (pmdaDoneCallBack)0);
    pmdaSetEndContextCallBack(dispatch, (pmdaEndContextCallBack)0);
}

/*
 * initialise the pmdaExt and pmdaInterface structures for a daemon
 * also set some globals
 */

void
pmdaDaemon(pmdaInterface *dispatch, int version, char *name, int domain, 
	   char *logfile, char *helptext)
{
    pmdaExt	*pmda = NULL;

    dispatch->domain = domain;
    __pmdaSetup(dispatch, version, name);

    if (dispatch->status < 0)
	return;

    if (HAVE_V_FOUR(version))
	pmda = dispatch->version.four.ext;
    else
	pmda = dispatch->version.two.ext;
    pmda->e_logfile = logfile;
    pmda->e_helptext = helptext;

    __pmSetInternalState(PM_STATE_PMCS);
}

/*
 * initialise the pmdaExt structure for a DSO
 * also set some globals
 */

void
pmdaDSO(pmdaInterface *dispatch, int version, char *name, char *helptext)
{
    __pmdaSetup(dispatch, version, name);

    if (dispatch->status < 0)
	return;

    if (HAVE_V_FOUR(version))
	dispatch->version.four.ext->e_helptext = helptext;
    else
	dispatch->version.two.ext->e_helptext = helptext;
}

/*
 * Redirect stderr to the log file
 */

void
pmdaOpenLog(pmdaInterface *dispatch)
{
    int c;

    if (dispatch->status != 0)
	return;

    if (!HAVE_V_FOUR(dispatch->comm.pmda_interface) &&
        !HAVE_V_TWO(dispatch->comm.pmda_interface)) {
	__pmNotifyErr(LOG_CRIT, "pmdaOpenLog: PMDA interface version %d not supported (domain=%d)",
		     dispatch->comm.pmda_interface, dispatch->domain);
	dispatch->status = PM_ERR_GENERIC;
	return;
    }

    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
	__pmOpenLog(dispatch->version.four.ext->e_name, 
    	       dispatch->version.four.ext->e_logfile, stderr, &c);
    else
	__pmOpenLog(dispatch->version.two.ext->e_name, 
    	       dispatch->version.two.ext->e_logfile, stderr, &c);
}
