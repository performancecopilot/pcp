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
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

/* MY_BUFLEN needs to big enough to hold "hostname port" */
#define MY_BUFLEN (MAXHOSTNAMELEN+10)
#define MY_VERSION "pmproxy-client 1\n"

static int
negotiate_proxy(int fd, const char *hostname, int port)
{
    char	buf[MY_BUFLEN];
    char	*bp;
    int		ok = 0;

    /*
     * version negotiation (converse to pmproxy logic)
     *   send my client version message
     *   recv server version message
     *   send hostname and port
     */

    if (send(fd, MY_VERSION, strlen(MY_VERSION), 0) != strlen(MY_VERSION)) {
	__pmNotifyErr(LOG_WARNING,
	     "__pmConnectPMCD: send version string to pmproxy failed: %s\n",
	     pmErrStr(-errno));
	return PM_ERR_IPC;
    }
    for (bp = buf; bp < &buf[MY_BUFLEN]; bp++) {
	if (recv(fd, bp, 1, 0) != 1) {
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
    if (send(fd, buf, strlen(buf), 0) != strlen(buf)) {
	__pmNotifyErr(LOG_WARNING,
	     "__pmConnectPMCD: send hostname+port string to pmproxy failed: %s'\n",
	     pmErrStr(-errno));
	return PM_ERR_IPC;
    }

    return ok;
}

/*
 * client connects to pmcd handshake
 */
int
__pmConnectHandshake(int fd)
{
    __pmPDU	*pb;
    int		ok;
    int		version;
    int		challenge;
    int		sts;

    /* Expect an error PDU back from PMCD: ACK/NACK for connection */
    sts = __pmGetPDU(fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
    if (sts == PDU_ERROR) {
	/*
	 * See comments in pmcd ... we actually get an extended PDU
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
	 * -1007 is magic and is PM_ERR_LICENSE for PCP 1.x.
	 * A 1.x pmcd will send us just the regular error PDU with
	 * a "status" value.
	 *
	 * NB: Licensing is a historical remnant from the earlier
	 * days of PCP on IRIX.  Modern day, open source PCP has no
	 * run-time licensing restrictions using this mechanism.
	 */
	ok = __pmDecodeXtendError(pb, PDU_BINARY, &sts, &challenge);
	if (ok < 0)
	    return ok;

	/*
	 * At this point, ok is PDU_VERSION1 or PDU_VERSION2 and
	 * sts is a PCP 2.0 error code
	 */
	version = ok;
	if ((ok = __pmSetVersionIPC(fd, version)) < 0)
	    return ok;

	if (version == PDU_VERSION1) {
	    /* 1.x pmcd */
	    ;
	}
	else if (sts < 0 && sts != PM_ERR_LICENSE) {
	    /* 2.0+ pmcd, but we have a fatal error on the connection ... */
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

	    handshake[0].c_type = CVERSION;
	    handshake[0].c_vala = PDU_VERSION;
	    handshake[0].c_valb = 0;
	    handshake[0].c_valc = 0;
	    sts = __pmSendCreds(fd, PDU_BINARY, 1, handshake);
	}
    }
    else
	sts = PM_ERR_IPC;

    return sts;
}

static int	global_nports;
static int	*global_portlist;
static int	default_portlist[] = { SERVER_PORT };

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
    int		sts = -1;
    int		fd = -1;	/* Fd for socket connection to pmcd */
    int		*ports;
    int		nports;
    int		i;
    int		version = -1;
    int		proxyport;
    pmHostSpec	*proxyhost;

    static int first_time = 1;
    static pmHostSpec proxy;

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
		if ((sts = __pmConnectHandshake(fd)) < 0) {
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
		    hosts[0].name, ports[i], fd, __pmVersionIPC(fd));
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
	else if ((sts = __pmConnectHandshake(fd)) < 0)
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
