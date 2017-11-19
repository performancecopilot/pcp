/*
 * Copyright (c) 1997-2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2012 Red Hat.  All Rights Reserved.
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

#include <signal.h>
#include "pmapi.h"
#include "libpcp.h"
#include "trace.h"
#include "trace_dev.h"

typedef struct {
    __pmTracePDU	*pdubuf;
    int			len;
} more_ctl;
static more_ctl		*more;
static int		maxfd = -1;

static char *
pdutypestr(int type)
{
    if (type == TRACE_PDU_ACK) return "ACK";
    else if (type == TRACE_PDU_DATA) return "DATA";
    else {
	static char     buf[20];
	pmsprintf(buf, sizeof(buf), "TYPE-%d?", type);
	return buf;
    }
}

static void
moreinput(int fd, __pmTracePDU *pdubuf, int len)
{
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_PDU) {
	__pmTracePDUHdr	*php = (__pmTracePDUHdr *)pdubuf;
	__pmTracePDU	*p;
	int		j, jend;
	char		*q;

	jend = (php->len+(int)sizeof(__pmTracePDU)-1)/(int)sizeof(__pmTracePDU);
	fprintf(stderr, "moreinput: fd=%d pdubuf=0x%p len=%d\n",
		fd, pdubuf, len);
	fprintf(stderr, "Piggy-back PDU: %s addr=0x%p len=%d from=%d",
		pdutypestr(php->type), php, php->len, php->from);
	fprintf(stderr, "%03d: ", 0);
	p = (__pmTracePDU *)php;

	/* for Purify ... */
	q = (char *)p + php->len;
	while (q < (char *)p + jend*sizeof(__pmTracePDU))
	    *q++ = '~'; /* buffer end */

	for (j = 0; j < jend; j++) {
	    if ((j % 8) == 0)
		fprintf(stderr, "\n%03d: ", j);
	    fprintf(stderr, "%8x ", p[j]);
	}
	putc('\n', stderr);
    }
#endif
    if (fd > maxfd) {
	int	next = maxfd + 1;
	if ((more = (more_ctl *)realloc(more, (fd+1)*sizeof(more[0]))) == NULL)
{
	    fprintf(stderr, "realloc failed (%d bytes): %s\n",
		    (fd+1)*(int)sizeof(more[0]), osstrerror());
	    return;
	}
	maxfd = fd;
	while (next <= maxfd) {
	    more[next].pdubuf = NULL;
	    next++;
	}
    }

    __pmtracepinPDUbuf(pdubuf);
    more[fd].pdubuf = pdubuf;
    more[fd].len = len;
}

int
__pmtracemoreinput(int fd)
{
    if (fd < 0 || fd > maxfd)
	return 0;

    return more[fd].pdubuf == NULL ? 0 : 1;
}

void
__pmtracenomoreinput(int fd)
{
    if (fd < 0 || fd > maxfd)
	return;

    if (more[fd].pdubuf != NULL) {
	__pmtraceunpinPDUbuf(more[fd].pdubuf);
	more[fd].pdubuf = NULL;
    }
}

static int
pduread(int fd, char *buf, int len, int mode, int timeout)
{
    /*
     * handle short reads that may split a PDU ...
     */
    int				status = 0;
    int				have = 0;
    __pmFdSet			onefd;
    static int			done_default = 0;
    static struct timeval	def_wait = { 10, 0 };

    if (timeout == TRACE_TIMEOUT_DEFAULT) {
	if (!done_default) {
	    double	def_timeout;
	    char	*timeout_str;
	    char	*end_ptr;

	    if ((timeout_str = getenv(TRACE_ENV_REQTIMEOUT)) != NULL) {
		def_timeout = strtod(timeout_str, &end_ptr);
		if (*end_ptr != '\0' || def_timeout < 0.0) {
		    status = PMTRACE_ERR_ENVFORMAT;
		    return status;
		}
		else {
		    pmtimevalFromReal(def_timeout, &def_wait);
		}
	    }
	    done_default = 1;
	}
    }

    while (len) {
	struct timeval	wait;
	/*
	 * either never timeout (i.e. block forever), or timeout
	 */
	if (timeout != TRACE_TIMEOUT_NEVER) {
	    if (timeout > 0) {
		wait.tv_sec = timeout;
		wait.tv_usec = 0;
	    }
	    else
		wait = def_wait;
	    __pmFD_ZERO(&onefd);
	    __pmFD_SET(fd, &onefd);
	    status = __pmSelectRead(fd+1, &onefd, &wait);
	    if (status == 0)
		return PMTRACE_ERR_TIMEOUT;
	    else if (status < 0) {
		setoserror(neterror());
		return status;
	    }
	}
	status = (int)__pmRead(fd, buf, len);
	if (status <= 0) {	/* EOF or error */
	    setoserror(neterror());
	    return status;
	}
	if (mode == -1)
	    /* special case, see __pmtracegetPDU */
	    return status;
	have += status;
	buf += status;
	len -= status;
    }

    return have;
}


/*
 * Because the default handler for SIGPIPE is to exit, we always want a handler
 * installed to override that so that the write() just returns an error.  The
 * problem is that the user might have installed one prior to the first write()
 * or may install one at some later stage.  This doesn't matter.  As long as a
 * handler other than SIG_DFL is there, all will be well.  The first time that
 * __pmtracexmitPDU is called, install SIG_IGN as the handler for SIGPIPE.
 * If the user had already changed the handler from SIG_DFL, put back what was
 * there before.
 */

int
__pmtracexmitPDU(int fd, __pmTracePDU *pdubuf)
{
    int			n, len;
    __pmTracePDUHdr	*php = (__pmTracePDUHdr *)pdubuf;

#if defined(HAVE_SIGPIPE)
    SIG_PF		user_onpipe;
    user_onpipe = signal(SIGPIPE, SIG_IGN);
    if (user_onpipe != SIG_DFL)	/* put user handler back */
	signal(SIGPIPE, user_onpipe);
#endif

    php->from = (__int32_t)getpid();
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_PDU) {
	int	j;
	int	jend = (php->len+(int)sizeof(__pmTracePDU)-1)/(int)sizeof(__pmTracePDU);
	char	*p;

	/* for Purify ... */
	p = (char *)pdubuf + php->len;
	while (p < (char *)pdubuf + jend*sizeof(__pmTracePDU))
	    *p++ = '~';	/* buffer end */

	fprintf(stderr, "[%d]__pmtracexmitPDU: %s fd=%d len=%d",
		php->from, pdutypestr(php->type), fd, php->len);
	for (j = 0; j < jend; j++) {
	    if ((j % 8) == 0)
		fprintf(stderr, "\n%03d: ", j);
	    fprintf(stderr, "%8x ", pdubuf[j]);
	}
	putc('\n', stderr);
    }
#endif
    len = php->len;

    php->len = htonl(php->len);
    php->from = htonl(php->from);
    php->type = htonl(php->type);
    n = (int)__pmWrite(fd, pdubuf, len);
    php->len = ntohl(php->len);
    php->from = ntohl(php->from);
    php->type = ntohl(php->type);

    if (n != len)
	return -oserror();

    return n;
}


int
__pmtracegetPDU(int fd, int timeout, __pmTracePDU **result)
{
    int			need, len;
    char		*handle;
    static int		maxsize = TRACE_PDU_CHUNK;
    __pmTracePDU	*pdubuf;
    __pmTracePDU	*pdubuf_prev;
    __pmTracePDUHdr	*php;

    /*
     *	This stuff is a little tricky.  What we try to do is read()
     *	an amount of data equal to the largest PDU we have (or are
     *	likely to have) seen thus far.  In the majority of cases
     *	this returns exactly one PDU's worth, i.e. read() returns
     *	a length equal to php->len.
     *
     *	For this to work, we have a special "mode" of -1
     *	to pduread() which means read, but return after the
     *	first read(), rather than trying to read up to the request
     *	length with multiple read()s, which would of course "hang"
     *	after the first PDU arrived.
     *
     *   We need to handle the following tricky cases:
     *   1. We get _more_ than we need for a single PDU -- happens
     *      when PDU's arrive together.  This requires "moreinput"
     *      to handle leftovers here (it gets even uglier if we
     *      have part, but not all of the second PDU).
     *   2. We get _less_ than we need for a single PDU -- this
     *      requires at least another read(), and possibly acquiring
     *      another pdubuf and doing a memcpy() for the partial PDU
     *      from the earlier call.
     */
    if (__pmtracemoreinput(fd)) {
	/* some leftover from last time ... handle -> start of PDU */
	pdubuf = more[fd].pdubuf;
	len = more[fd].len;
	__pmtracenomoreinput(fd);
    }
    else {
	if ((pdubuf = __pmtracefindPDUbuf(maxsize)) == NULL)
	    return -oserror();
	len = pduread(fd, (void *)pdubuf, maxsize, -1, timeout);
    }
    php = (__pmTracePDUHdr *)pdubuf;

    if (len < (int)sizeof(__pmTracePDUHdr)) {
	if (len == -1) {
	    if (oserror() == ECONNRESET||
		oserror() == ETIMEDOUT || oserror() == ENETDOWN ||
		oserror() == ENETUNREACH || oserror() == EHOSTDOWN ||
		oserror() == EHOSTUNREACH || oserror() == ECONNREFUSED)
		/*
		 * failed as a result of pmdatrace exiting and the
		 * connection being reset, or as a result of the kernel
		 * ripping down the connection (most likely because the
		 * host at the other end just took a dive)
		 *
		 * treat this like end of file on input
		 *
		 * from irix/kern/fs/nfs/bds.c seems like all of the
		 * following are peers here:
		 *  ECONNRESET (pmdatrace terminated?)
		 *  ETIMEDOUT ENETDOWN ENETUNREACH EHOSTDOWN EHOSTUNREACH
		 *  ECONNREFUSED
		 * peers for bds but not here:
		 *  ENETRESET ENONET ESHUTDOWN (cache_fs only?)
		 *  ECONNABORTED (accept, user req only?)
		 *  ENOTCONN (udp?)
		 *  EPIPE EAGAIN (nfs, bds & ..., but not ip or tcp?)
		 */
		len = 0;
	    else
		fprintf(stderr, "__pmtracegetPDU: fd=%d hdr: %s",
			fd, osstrerror());
	}
	else if (len > 0)
	    fprintf(stderr, "__pmtracegetPDU: fd=%d hdr: len=%d, not %d?",
			fd, len, (int)sizeof(__pmTracePDUHdr));
	else if (len == PMTRACE_ERR_TIMEOUT)
	    return PMTRACE_ERR_TIMEOUT;
	else if (len < 0)
	    fprintf(stderr, "__pmtracegetPDU: fd=%d hdr: %s", fd, pmtraceerrstr(len));
	return len ? PMTRACE_ERR_IPC : 0;
    }

    php->len = ntohl(php->len);
    if (php->len < 0) {
	fprintf(stderr, "__pmtracegetPDU: fd=%d illegal len=%d in hdr\n", fd, php->len);
	return PMTRACE_ERR_IPC;
    }

    if (len == php->len)
	/* return below */
	;
    else if (len > php->len) {
	/*
	 * read more than we need for this one, save it up for next time
	 */
	handle = (char *)pdubuf;
	moreinput(fd, (__pmTracePDU *)&handle[php->len], len - php->len);
    }
    else {
	int	tmpsize;

	/*
	 * need to read more ...
	 */
	__pmtracepinPDUbuf(pdubuf);
	pdubuf_prev = pdubuf;
	if (php->len > maxsize)
	    tmpsize = TRACE_PDU_CHUNK * ( 1 + php->len / TRACE_PDU_CHUNK);
	else
	    tmpsize = maxsize;
	if ((pdubuf = __pmtracefindPDUbuf(tmpsize)) == NULL) {
	    __pmtraceunpinPDUbuf(pdubuf_prev);
	    return -oserror();
	}
	if (php->len > maxsize)
	    maxsize = tmpsize;
	memmove((void *)pdubuf, (void *)php, len);
	__pmtraceunpinPDUbuf(pdubuf_prev);
	php = (__pmTracePDUHdr *)pdubuf;
	need = php->len - len;
	handle = (char *)pdubuf;
	/* block until all of the PDU is received this time */
	len = pduread(fd, (void *)&handle[len], need, 0, timeout);
	if (len != need) {
	    if (len == PMTRACE_ERR_TIMEOUT)
		return PMTRACE_ERR_TIMEOUT;
	    if (len < 0)
		fprintf(stderr, "__pmtracegetPDU: error (%d) fd=%d: %s\n", (int)oserror(), fd, osstrerror());
	    else
		fprintf(stderr, "__pmtracegetPDU: len=%d, not %d? (fd=%d)\n", len, need, fd);
	    fprintf(stderr, "hdr: len=0x%08x type=0x%08x from=0x%08x\n",
			php->len, (int)ntohl(php->type), (int)ntohl(php->from));
	    return PMTRACE_ERR_IPC;
	}
    }

    *result = (__pmTracePDU *)php;
    php->type = ntohl(php->type);
    php->from = ntohl(php->from);
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_PDU) {
	int		j;
	int		jend = (int)(php->len+(int)sizeof(__pmTracePDU)-1)/(int)sizeof(__pmTracePDU);
	char	*p;

	/* for Purify ... */
	p = (char *)*result + php->len;
	while (p < (char *)*result + jend*sizeof(__pmTracePDU))
	    *p++ = '~';	/* buffer end */

	fprintf(stderr, "[%" FMT_PID "]__pmtracegetPDU: %s fd=%d len=%d from=%d",
		(pid_t)getpid(), pdutypestr(php->type), fd, php->len, php->from);

	for (j = 0; j < jend; j++) {
	    if ((j % 8) == 0)
		fprintf(stderr, "\n%03d: ", j);
	    fprintf(stderr, "%8x ", (*result)[j]);
	}
	putc('\n', stderr);
    }
#endif

    return php->type;
}
