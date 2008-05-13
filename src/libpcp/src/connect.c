/*
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: connect.c,v 1.10 2006/05/02 05:57:28 makc Exp $"

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <syslog.h>

#if defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#endif

#include "dsotbl.h"

/*
 * for libpcp and unauthorized clients, __pmConnectHostMethod() leads
 * here and PMCD connection failure ...
 */
/*ARGSUSED*/
static int
pcp_trustme(int fd, int what)
{
#ifndef HAVE_FLEXLM
#ifdef HAVE_DLOPEN
    char libpath[MAXPATHLEN];
    void * dso;

    snprintf (libpath, sizeof(libpath), "%s/libpcp_mon.so", pmGetConfig("PCP_LIB_DIR"));
	
    if ( (dso = dlopen (libpath, RTLD_LAZY)) != NULL ) {
	void * sym;

	if ( (sym = dlsym (dso, "__pmSetAuthClient")) != NULL ) {
	    void (*f) (void) = (void (*) (void))sym;
	    (*f)();
	    /* No recursion here - __pmSetAuthClient MUST change the
	     * pointer ... */
	    return (__pmConnectHostMethod (fd, what));
	} else {
	    dlclose (dso);
	}
    }	    
#endif
#endif
    return PM_ERR_PMCDLICENSE;
}

/*
 * Function pointer manipulation here is used to make the authorised
 * pmcd connect functionality accessible via a single call to
 * __pmConnectPMCD and in such a way that all subsequent calls will do
 * the right thing (always) when deciding which to call.
 */
__pmConnectHostType __pmConnectHostMethod = pcp_trustme;

/* MY_BUFLEN needs to big enough to hold "hostname port" */
#define MY_BUFLEN (MAXHOSTNAMELEN+10)
#define MY_VERSION "pmproxy-client 1\n"

static int
negotiate_proxy(int fd, const char *hostname, int port)
{
    char	buf[MY_BUFLEN];
    char	*bp;
    int		ok = 0;
    extern int	errno;

    /*
     * version negotiation (converse to pmproxy logic)
     *   send my client version message
     *   recv server version message
     *   send hostname and port
     */

    if (write(fd, MY_VERSION, strlen(MY_VERSION)) != strlen(MY_VERSION)) {
	__pmNotifyErr(LOG_WARNING,
	     "__pmConnectPMCD: send version string to pmproxy failed: %s\n",
	     pmErrStr(-errno));
	return PM_ERR_IPC;
    }
    for (bp = buf; bp < &buf[MY_BUFLEN]; bp++) {
	if (read(fd, bp, 1) != 1) {
	    *bp = '\0';
	    bp = &buf[MY_BUFLEN];
	    break;
	}
	if (*bp == '\n' || *bp == '\r') {
	    *bp = '\0';
	    break;
	}
    }
    if (bp < &buf[MY_BUFLEN]) {
	if (strcmp(buf, "pmproxy-server 1") == 0)
	    ok = 1;
    }

    if (!ok) {
	__pmNotifyErr(LOG_WARNING,
	     "__pmConnectPMCD: bad version string from pmproxy: \"%s\"\n",
	    buf);
	return PM_ERR_IPC;
    }

    snprintf(buf, sizeof(buf), "%s %d\n", hostname, port);
    if (write(fd, buf, strlen(buf)) != strlen(buf)) {
	__pmNotifyErr(LOG_WARNING,
	     "__pmConnectPMCD: send hostname+port string to pmproxy failed: %s'\n",
	     pmErrStr(-errno));
	return PM_ERR_IPC;
    }

    return ok;
}

#ifndef HAVE_UNSETENV
/*
 * don't have unsetenv() for IRIX, so do it the long-handed portable way
 */
static int
unsetenv(const char *name)
{
    extern char **_environ;
    char	**ep;
    int		len = (int)strlen(name);
    int		found = 0;

    for (ep = _environ; *ep != NULL; ep++) {
	if (strncmp(*ep, name, len) == 0 && (*ep)[len] == '=') {
	    found = 1;
	}
	if (found)
	    ep[0] = ep[1];
    }
    return found;
}
#endif

/*
 * client connects to pmcd handshake
 */
static int
do_handshake(int fd, __pmIPC *infop)
{
    __pmPDU	*pb;
    int		ok;
    int		challenge;
    int		sts;

    /* Expect an error PDU back from PMCD: ACK/NACK for connection */
    sts = __pmGetPDU(fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
    if (sts == PDU_ERROR) {
	/*
	 * see comments in pmcd ... we actually get an extended PDU
	 * from a 2.0 pmcd, of the form
	 *
	 *  :----------:-----------:
	 *  |  status  | challenge |
	 *  :----------:-----------:
	 *
	 *                            status      challenge
	 *     pmcd  licensed             0          bits
	 *	     unlicensed       -1007          bits
	 *
	 *  -1007 is magic and is PM_ERR_LICENSE for PCP 1.x
	 *
	 * a 1.x pmcd will send us just the regular error PDU with
	 * a "status" value.
	 */
	ok = __pmDecodeXtendError(pb, PDU_BINARY, &sts, &challenge);
	if (ok < 0) {
	    return ok;
	}

	/*
	 * At this point, ok is PDU_VERSION1 or PDU_VERSION2 and
	 * sts is a PCP 2.0 error code
	 */
	infop->version = ok;
	infop->ext = NULL;
	if ((ok = __pmAddIPC(fd, *infop)) < 0) {
	    return ok;
	}

	if (infop->version == PDU_VERSION1) {
	    /* 1.x pmcd */
	    ;
	}
	else if (sts < 0 && sts != PM_ERR_LICENSE) {
	    /*
	     * 2.0+ pmcd, but we have a fatal error on the connection ...
	     */
	    ;
	}
	else {
	    /*
	     * 2.0+ pmcd, either pmcd is not licensed or no error so far,
	     * so negotiate connection version and credentials
	     */
	    __pmPDUInfo	*pduinfo;
	    __pmCred	handshake[2];

	    /*
	     * note: __pmDecodeXtendError() has not swabbed challenge
	     * because it does not know it's data type.
	     */
	    pduinfo = (__pmPDUInfo *)&challenge;
	    *pduinfo = __ntohpmPDUInfo(*pduinfo);

	    if (pduinfo->licensed == PM_LIC_COL) {
		/* licensed pmcd, accept all */
		handshake[0].c_type = CVERSION;
		handshake[0].c_vala = PDU_VERSION;
		handshake[0].c_valb = 0;
		handshake[0].c_valc = 0;
		sts = __pmSendCreds(fd, PDU_BINARY, 1, handshake);
	    }
	    else
		/* pmcd not licensed, are you an authorized client? */
		sts = __pmConnectHostMethod(fd, challenge);
	}
    }
    else
	sts = PM_ERR_IPC;

    return sts;
}

int
__pmConnectHandshake (int fd)
{
   __pmIPC	ipcinfo = { UNKNOWN_VERSION, NULL };
   return (do_handshake (fd, &ipcinfo));
}

static int	global_nports;
static int	*global_portlist;
static int	default_portlist[] = { SERVER_PORT, OLD_SERVER_PORT };

static void
load_pmcd_ports(void)
{
    static int	first_time = 1;

    if (first_time) {
	char	*envstr;
	char	*endptr;

	first_time = 0;

	if ((envstr = getenv("PMCD_PORT")) != NULL) {
	    char	*p = envstr;

	    for ( ; ; ) {
		int size, port = (int)strtol(p, &endptr, 0);
		if ((*endptr != '\0' && *endptr != ',') || port < 0) {
		    __pmNotifyErr(LOG_WARNING,
				  "ignored bad PMCD_PORT = '%s'", p);
		}
		else {
		    size = ++global_nports * sizeof(int);
		    global_portlist = (int *)realloc(global_portlist, size);
		    if (global_portlist == NULL) {
			__pmNotifyErr(LOG_WARNING,
				     "__pmConnectPMCD: portlist alloc failed (%d bytes), using default PMCD_PORT (%d)\n", size, SERVER_PORT);
			global_nports = 0;
			break;
		    }
		    global_portlist[global_nports-1] = port;
		}
		if (*endptr == '\0')
		    break;
		p = &endptr[1];
	    }
	}

	if (global_nports == 0) {
	    global_portlist = default_portlist;
	    global_nports = sizeof(default_portlist) / sizeof(default_portlist[0]);
	}
    }
}

void
__pmConnectGetPorts(pmHostSpec *host)
{
    load_pmcd_ports();
    if (__pmAddHostPorts(host, global_portlist, global_nports) < 0) {
	__pmNotifyErr(LOG_WARNING,
		"__pmConnectGetPorts: portlist dup failed, "
		"using default PMCD_PORT (%d)\n", SERVER_PORT);
	host->ports[0] = SERVER_PORT;
	host->nports = 1;
    }
}

int
__pmConnectPMCD(pmHostSpec *hosts, int nhosts)
{
    __pmIPC	ipcinfo = { UNKNOWN_VERSION, NULL };
    int		sts;
    int		fd;	/* Fd for socket connection to pmcd */
    int		*ports;
    int		nports;
    int		i;
    int		version;
    int		proxyport;
    pmHostSpec	*proxyhost;

    static int first_time = 1;
    static pmHostSpec proxy;
    extern int errno;

    if (first_time) {
	/*
	 * One-trip check for use of pmproxy(1) in lieu of pmcd(1),
	 * and to extract the optional environment variables ...
	 * PMCD_PORT, PMPROXY_HOST and PMPROXY_PORT
	 */
	char	*envstr;
	char	*endptr;

	first_time = 0;

	load_pmcd_ports();

	if ((envstr = getenv("PMPROXY_HOST")) != NULL) {
	    proxy.name = strdup(envstr);
	    if (proxy.name == NULL) {
		__pmNotifyErr(LOG_WARNING,
			     "__pmConnectPMCD: cannot save PMPROXY_HOST: %s\n",
			     pmErrStr(-errno));
	    }
	    else {
		static int proxy_port = PROXY_PORT;
		if ((envstr = getenv("PMPROXY_PORT")) != NULL) {
		    proxy_port = (int)strtol(envstr, &endptr, 0);
		    if (*endptr != '\0' || proxy_port < 0) {
			__pmNotifyErr(LOG_WARNING,
			    "__pmConnectPMCD: ignored bad PMPROXY_PORT = '%s'\n", envstr);
			proxy_port = PROXY_PORT;
		    }
		}
		proxy.ports = &proxy_port;
		proxy.nports = 1;
	    }
	}
    }

    if (hosts[0].nports > 0) {
	nports = hosts[0].nports;
	ports = hosts[0].ports;
    }
    else {
	nports = global_nports;
	ports = global_portlist;
    }

    if (proxy.name == NULL && nhosts == 1) {
	/*
	 * no proxy, connecting directly to pmcd
	 */
	for (i = 0; i < nports; i++) {
	    if ((fd = __pmAuxConnectPMCDPort(hosts[0].name, ports[i])) >= 0) {
		if ((sts = do_handshake(fd, &ipcinfo)) < 0) {
		    close(fd);
		}
		else
		    /* success */
		    break;
	    }
	    else
		sts = fd;
	}

	if (sts < 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT) {
		fprintf(stderr, "__pmConnectPMCD(%s): pmcd connection port=",
		   hosts[0].name);
		for (i = 0; i < nports; i++) {
		    if (i == 0) fprintf(stderr, "%d", ports[i]);
		    else fprintf(stderr, ",%d", ports[i]);
		}
		fprintf(stderr, " failed: %s\n", pmErrStr(sts));
	    }
#endif
	    return sts;
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "__pmConnectPMCD(%s): pmcd connection port=%d fd=%d PDU version=%u\n",
		    hosts[0].name, ports[i], fd, ipcinfo.version);
	    __pmPrintIPC();
	}
#endif

	return fd;
    }

    /*
     * connecting to pmproxy, and then to pmcd ... not a direct
     * connection to pmcd
     */
    proxyhost = (nhosts > 1) ? &hosts[1] : &proxy;
    proxyport = (proxyhost->nports > 0) ? proxyhost->ports[0] : PROXY_PORT;

    for (i = 0; i < nports; i++) {
	fd = __pmAuxConnectPMCDPort(proxyhost->name, proxyport);
	if (fd < 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT) {
		fprintf(stderr, "__pmConnectPMCD(%s): proxy to %s port=%d failed: %s \n",
			hosts[0].name, proxyhost->name, proxyport, pmErrStr(-errno));
	    }
#endif
	    return fd;
	}
	if ((sts = version = negotiate_proxy(fd, hosts[0].name, ports[i])) < 0)
	    close(fd);
	else if ((sts = do_handshake(fd, &ipcinfo)) < 0)
	    close(fd);
	else
	    /* success */
	    break;
    }

    if (sts < 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "__pmConnectPMCD(%s): proxy connection to %s port=",
			hosts[0].name, proxyhost->name);
	    for (i = 0; i < nports; i++) {
		if (i == 0) fprintf(stderr, "%d", ports[i]);
		else fprintf(stderr, ",%d", ports[i]);
	    }
	    fprintf(stderr, " failed: %s\n", pmErrStr(sts));
	}
#endif
	return sts;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "__pmConnectPMCD(%s): proxy connection host=%s port=%d fd=%d version=%d\n",
	    hosts[0].name, proxyhost->name, ports[i], fd, version);
    }
#endif
    return fd;
}

__pmDSO *
__pmLookupDSO(int domain)
{
    int		i;
    for (i = 0; i < numdso; i++) {
	if (dsotab[i].domain == domain && dsotab[i].handle != NULL)
	    return &dsotab[i];
    }
    return NULL;
}

int
__pmConnectLocal(void)
{
    static int		done_init = 0;
    int			i;
    __pmDSO		*dp;
    char		pathbuf[MAXPATHLEN];
    const char		*path;
    unsigned int	challenge;
    void		(*initp)(pmdaInterface *);

    if (done_init)
	return 0;

    for (i = 0; i < numdso; i++) {
	dp = &dsotab[i];
	if (dp->domain == SAMPLE_DSO) {
	    /*
	     * only attach sample pmda dso if env var PCP_LITE_SAMPLE
	     * or PMDA_LOCAL_SAMPLE is set
	     */
	    if (getenv("PCP_LITE_SAMPLE") == NULL &&
		getenv("PMDA_LOCAL_SAMPLE") == NULL) {
		/* no sample pmda */
		dp->domain = -1;
		continue;
	    }
	}
#ifdef PROC_DSO
	/*
	 * For Linux (and perhaps anything other than IRIX), the proc
	 * PMDA is part of the OS PMDA, so this one cannot be optional
	 * ... the makefile will ensure dsotbl.h is set up correctly
	 * and PROC_DSO will or will not be defined as required
	 */
	if (dp->domain == PROC_DSO) {
	    /*
	     * only attach proc pmda dso if env var PMDA_LOCAL_PROC
	     * is set
	     */
	    if (getenv("PMDA_LOCAL_PROC") == NULL) {
		/* no proc pmda */
		dp->domain = -1;
		continue;
	    }
	}
#endif

        snprintf(pathbuf, sizeof(pathbuf), "%s/%s",
		 pmGetConfig("PCP_PMDAS_DIR"), dp->name);
	if ((path = __pmFindPMDA(pathbuf)) == NULL) {
	    pmprintf("__pmConnectLocal: Warning: cannot find DSO \"%s\"\n", 
		     pathbuf);
	    pmflush();
	    dp->domain = -1;
	    dp->handle = NULL;
	}
	else {
#ifdef HAVE_DLOPEN
            dp->handle = dlopen(path, RTLD_NOW);
	    if (dp->handle == NULL) {
		pmprintf("__pmConnectLocal: Warning: error attaching DSO "
			 "\"%s\"\n%s\n\n", path, dlerror());
		pmflush();
		dp->domain = -1;
	    }
#else	/* ! HAVE_DLOPEN */
	    dp->handle = NULL;
	    pmprintf("__pmConnectLocal: Warning: error attaching DSO \"%s\"\n", path);
	    pmprintf("No dynamic DSO/DLL support on this platform\n\n");
	    pmflush();
	    dp->domain = -1;
#endif
	}

	if (dp->handle == NULL)
	    continue;

#ifdef HAVE_DLOPEN
	/*
	 * rest of this only makes sense if the dlopen() worked
	 */
	initp = (void (*)(pmdaInterface *))dlsym(dp->handle, dp->init);
	if (initp == NULL) {
	    pmprintf("__pmConnectLocal: Warning: couldn't find init function "
		     "\"%s\" in DSO \"%s\"\n", dp->init, path);
	    pmflush();
	    dlclose(dp->handle);
	    dp->domain = -1;
	    continue;
	}

	/*
	 * Pass in the expected domain id.
	 * The PMDA initialization routine can (a) ignore it, (b) check it
	 * is the expected value, or (c) self-adapt.
	 */
	dp->dispatch.domain = dp->domain;

	/*
	 * the PMDA interface / PMAPI version discovery as a "challenge" ...
	 * for pmda_interface it is all the bits being set,
	 * for pmapi_version it is the complement of the one you are using now
	 */
	challenge = 0xff;
	dp->dispatch.comm.pmda_interface = challenge;
	dp->dispatch.comm.pmapi_version = ~PMAPI_VERSION;

	dp->dispatch.comm.flags = 0;
	dp->dispatch.status = 0;

	(*initp)(&dp->dispatch);

	if (dp->dispatch.status != 0) {
	    /* initialization failed for some reason */
	    pmprintf("__pmConnectLocal: Warning: initialization "
		     "routine \"%s\" failed in DSO \"%s\": %s\n", 
		     dp->init, path, pmErrStr(dp->dispatch.status));
	    pmflush();
	    dlclose(dp->handle);
	    dp->domain = -1;
	}
	else {
	    if (dp->dispatch.comm.pmda_interface == challenge) {
		/*
		 * DSO did not change pmda_interface, assume PMAPI version 1
		 * from PCP 1.x and PMDA_INTERFACE_1
		 */
		dp->dispatch.comm.pmda_interface = PMDA_INTERFACE_1;
		dp->dispatch.comm.pmapi_version = PMAPI_VERSION_1;
	    }
	    else {
		/*
		 * gets a bit tricky ...
		 * interface_version (8-bits) used to be version (4-bits),
		 * so it is possible that only the bottom 4 bits were
		 * changed and in this case the PMAPI version is 1 for
		 * PCP 1.x
		 */
		if ((dp->dispatch.comm.pmda_interface & 0xf0) == (challenge & 0xf0)) {
		    dp->dispatch.comm.pmda_interface &= 0x0f;
		    dp->dispatch.comm.pmapi_version = PMAPI_VERSION_1;
		}
	    }

	    if (dp->dispatch.comm.pmda_interface < PMDA_INTERFACE_1 ||
		dp->dispatch.comm.pmda_interface > PMDA_INTERFACE_LATEST) {
		pmprintf("__pmConnectLocal: Error: Unknown PMDA interface "
			 "version %d in \"%s\" DSO\n", 
			 dp->dispatch.comm.pmda_interface, path);
		pmflush();
		dlclose(dp->handle);
		dp->domain = -1;
	    }

	    if (dp->dispatch.comm.pmapi_version != PMAPI_VERSION_1 &&
		dp->dispatch.comm.pmapi_version != PMAPI_VERSION_2) {
		pmprintf("__pmConnectLocal: Error: Unknown PMAPI version %d "
			 "in \"%s\" DSO\n",
			 dp->dispatch.comm.pmapi_version, path);
		pmflush();
		dlclose(dp->handle);
		dp->domain = -1;
	    }
	}
#endif	/* HAVE_DLOPEN */

    }

    done_init = 1;

    return 0;
}
