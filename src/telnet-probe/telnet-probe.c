/*
 * Lightweight telnet clone for shping and espping PMDAs.
 *
 * Usage: telnet-probe [-v] host port
 *
 * Once telnet connection is established:
 *	read stdin until EOF, writing to telnet connection
 *	read telnet until EOF, discarding data
 * -c (connect only) flag skips the send-receive processing
 *
 * Exit status is 1 in the case of any errors, else 0.
 */

#include "pmapi.h"
#include "libpcp.h"

int
main(int argc, char *argv[])
{
    __pmFdSet		wfds;
    __pmSockAddr	*myAddr;
    __pmHostEnt		*servInfo;
    void		*enumIx;
    int			flags = 0;
    char		*endnum;
    int			port = 0;
    int			s;
    int			errflag = 0;
    int			cflag = 0;
    int			vflag = 0;
    int			sts = 1;
    ssize_t		bytes;
    int			ret;
    struct timeval	canwait = { 5, 000000 };
    struct timeval	stv;
    struct timeval	*pstv;
    int			c;
    char		cc;

    while ((c = getopt(argc, argv, "cv?")) != EOF) {
        switch (c) {
	case 'c':
	    cflag = 1;
	    break;
	case 'v':
	    vflag = 1;
	    break;
	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (optind+2 != argc) {
	fprintf(stderr, "%s: requires two arguments\n", argv[0]);
	errflag++;
    }
    else {
	port = (int)strtol(argv[optind+1], &endnum, 10);
	if (*endnum != '\0' || port < 0) {
	    fprintf(stderr, "%s: port must be a positive number\n", argv[0]);
	    errflag++;
	}
    }
    if (errflag) {
	fprintf(stderr, "Usage: %s [-c] [-v] host port\n", argv[0]);
	goto done;
    }

    if ((servInfo = __pmGetAddrInfo(argv[optind])) == NULL) {
	if (vflag)
	    fprintf(stderr, "__pmGetAddrInfo: %s\n", hoststrerror());
	goto done;
    }

    s = -1;
    enumIx = NULL;
    for (myAddr = __pmHostEntGetSockAddr(servInfo, &enumIx);
	 myAddr != NULL;
	 myAddr = __pmHostEntGetSockAddr(servInfo, &enumIx)) {
	/* Create a socket */
	if (__pmSockAddrIsInet(myAddr))
	    s = __pmCreateSocket();
	else if (__pmSockAddrIsIPv6(myAddr))
	    s = __pmCreateIPv6Socket();
	else
	    continue;
	if (s < 0) {
	    __pmSockAddrFree(myAddr);
	    continue; /* Try the next address */
	}

	/* Attempt to connect */
	flags = __pmConnectTo(s, myAddr, port);
	__pmSockAddrFree(myAddr);

	if (flags < 0) {
	    /*
	     * Mark failure in case we fall out the end of the loop
	     * and try next address. s has been closed in __pmConnectTo().
	     */
	    setoserror(ECONNREFUSED);
	    s = -1;
	    continue;
	}

	/* FNDELAY and we're in progress - wait on select */
	stv = canwait;
	pstv = (stv.tv_sec || stv.tv_usec) ? &stv : NULL;
	__pmFD_ZERO(&wfds);
	__pmFD_SET(s, &wfds);
	ret = __pmSelectWrite(s+1, &wfds, pstv);

	/* Was the connection successful? */
	if (ret == 0)
	    setoserror(ETIMEDOUT);
	else if (ret > 0) {
	    ret = __pmConnectCheckError(s);
	    if (ret == 0)
		break;
	    setoserror(ret);
	}

	/* Unsuccessful connection. */
	__pmCloseSocket(s);
	s = -1;
    } /* loop over addresses */

    __pmHostEntFree(servInfo);

    if (s != -1)
	s = __pmConnectRestoreFlags(s, flags);
    if (s < 0) {
	if (vflag)
	    fprintf(stderr, "connect: %s\n", netstrerror());
	goto done;
    }

    if (cflag) {
	/* skip send-recv exercise */
	sts = 0;
	goto done;
    }

    if (vflag)
	fprintf(stderr, "send ...\n");
    while ((c = getc(stdin)) != EOF) {
	/* NB: copy only the payload byte, not the whole int */
	cc = (char) c;
	if (vflag) {
	    fputc((int)cc, stderr);
	    fflush(stderr);
	}
	if (__pmWrite(s, &cc, sizeof(cc)) != sizeof(cc)) {
	    if (vflag)
		fprintf(stderr, "telnet write: %s\n", osstrerror());
	    goto done;
	}
    }

    if (vflag)
	fprintf(stderr, "recv ...\n");
    /* NB: read one char, not int, at a time */
    while ((bytes = __pmRead(s, &cc, sizeof(cc))) == sizeof(cc)) {
	if (vflag) {
	    fputc((int)cc, stderr);
	    fflush(stderr);
	}
    }
    if (bytes < 0) {
	/*
	 * If __pmSocketClosed(), then treat it as EOF.
	 */
	if (! __pmSocketClosed()) {
	    if (vflag)
		fprintf(stderr, "telnet read: %s\n", osstrerror());
	    goto done;
	}
    }

    sts = 0;

done:
    if (vflag)
	fprintf(stderr, "exit: %d\n", sts);
    exit(sts);
}
